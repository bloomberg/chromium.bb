// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_controller.h"

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/completion_event.h"
#include "cc/tiles/tile_task_manager.h"

namespace cc {

ImageController::ImageDecodeRequestId
    ImageController::s_next_image_decode_queue_id_ = 1;

ImageController::ImageController(
    base::SequencedTaskRunner* origin_task_runner,
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner)
    : origin_task_runner_(origin_task_runner),
      worker_task_runner_(std::move(worker_task_runner)),
      weak_ptr_factory_(this) {}

ImageController::~ImageController() {
  StopWorkerTasks();
}

void ImageController::StopWorkerTasks() {
  // We can't have worker threads without a cache_ or a worker_task_runner_, so
  // terminate early.
  if (!cache_ || !worker_task_runner_)
    return;

  // Abort all tasks that are currently scheduled to run (we'll wait for them to
  // finish next).
  {
    base::AutoLock hold(lock_);
    abort_tasks_ = true;
  }

  // Post a task that will simply signal a completion event to ensure that we
  // "flush" any scheduled tasks (they will abort).
  CompletionEvent completion_event;
  worker_task_runner_->PostTask(
      FROM_HERE, base::Bind([](CompletionEvent* event) { event->Signal(); },
                            base::Unretained(&completion_event)));
  completion_event.Wait();

  // Reset the abort flag so that new tasks can be scheduled.
  {
    base::AutoLock hold(lock_);
    abort_tasks_ = false;
  }

  // Now that we flushed everything, if there was a task running and it
  // finished, it would have posted a completion callback back to the compositor
  // thread. We don't want that, so invalidate the weak ptrs again. Note that
  // nothing can start running between wait and this invalidate, since it would
  // only run on the current (compositor) thread.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Now, begin cleanup.

  // Unlock all of the locked images (note that this vector would only be
  // populated if we actually need to unref the image.
  for (auto image_pair : requested_locked_images_)
    cache_->UnrefImage(image_pair.second);
  requested_locked_images_.clear();

  // Now, complete the tasks that already ran but haven't completed. These would
  // be posted in the run loop, but since we invalidated the weak ptrs, we need
  // to run everything manually.
  for (auto& request_to_complete : requests_needing_completion_) {
    ImageDecodeRequestId id = request_to_complete.first;
    ImageDecodeRequest& request = request_to_complete.second;

    // The task (if one exists) would have run already, so we just need to
    // complete it.
    if (request.task)
      request.task->DidComplete();

    // Issue the callback, and unref the image immediately. This is so that any
    // code waiting on the callback can proceed, although we're breaking the
    // promise of having this image decoded. This is unfortunate, but it seems
    // like the least complexity to process an image decode controller becoming
    // nullptr.
    request.callback.Run(id);
    if (request.need_unref)
      cache_->UnrefImage(request.draw_image);
  }
  requests_needing_completion_.clear();

  // Finally, complete all of the tasks that never started running. This is
  // similar to the |requests_needing_completion_|, but happens at a different
  // stage in the pipeline.
  for (auto& request_pair : image_decode_queue_) {
    ImageDecodeRequestId id = request_pair.first;
    ImageDecodeRequest& request = request_pair.second;

    if (request.task) {
      // This task may have run via a different request, so only cancel it if
      // it's "new". That is, the same task could have been referenced by
      // several different image deque requests for the same image.
      if (request.task->state().IsNew())
        request.task->state().DidCancel();
      request.task->DidComplete();
    }
    // Run the callback and unref the image.
    request.callback.Run(id);
    cache_->UnrefImage(request.draw_image);
  }
  image_decode_queue_.clear();
}

void ImageController::SetImageDecodeCache(ImageDecodeCache* cache) {
  if (!cache) {
    SetPredecodeImages(std::vector<DrawImage>(),
                       ImageDecodeCache::TracingInfo());
    StopWorkerTasks();
  }
  cache_ = cache;
}

void ImageController::GetTasksForImagesAndRef(
    std::vector<DrawImage>* images,
    std::vector<scoped_refptr<TileTask>>* tasks,
    const ImageDecodeCache::TracingInfo& tracing_info) {
  DCHECK(cache_);
  for (auto it = images->begin(); it != images->end();) {
    scoped_refptr<TileTask> task;
    bool need_to_unref_when_finished =
        cache_->GetTaskForImageAndRef(*it, tracing_info, &task);
    if (task)
      tasks->push_back(std::move(task));

    if (need_to_unref_when_finished)
      ++it;
    else
      it = images->erase(it);
  }
}

void ImageController::UnrefImages(const std::vector<DrawImage>& images) {
  for (auto image : images)
    cache_->UnrefImage(image);
}

void ImageController::ReduceMemoryUsage() {
  DCHECK(cache_);
  cache_->ReduceCacheUsage();
}

std::vector<scoped_refptr<TileTask>> ImageController::SetPredecodeImages(
    std::vector<DrawImage> images,
    const ImageDecodeCache::TracingInfo& tracing_info) {
  std::vector<scoped_refptr<TileTask>> new_tasks;
  GetTasksForImagesAndRef(&images, &new_tasks, tracing_info);
  UnrefImages(predecode_locked_images_);
  predecode_locked_images_ = std::move(images);
  return new_tasks;
}

ImageController::ImageDecodeRequestId ImageController::QueueImageDecode(
    sk_sp<const SkImage> image,
    const ImageDecodedCallback& callback) {
  // We must not receive any image requests if we have no worker.
  CHECK(worker_task_runner_);

  // Generate the next id.
  ImageDecodeRequestId id = s_next_image_decode_queue_id_++;

  DCHECK(image);
  auto image_bounds = image->bounds();
  DrawImage draw_image(std::move(image), image_bounds, kNone_SkFilterQuality,
                       SkMatrix::I());

  // Get the tasks for this decode.
  scoped_refptr<TileTask> task;
  bool need_unref =
      cache_->GetOutOfRasterDecodeTaskForImageAndRef(draw_image, &task);
  // If we don't need to unref this, we don't actually have a task.
  DCHECK(need_unref || !task);

  // Schedule the task and signal that there is more work.
  base::AutoLock hold(lock_);
  image_decode_queue_[id] =
      ImageDecodeRequest(id, draw_image, callback, std::move(task), need_unref);

  // If this is the only image decode request, schedule a task to run.
  // Otherwise, the task will be scheduled in the previou task's completion.
  if (image_decode_queue_.size() == 1) {
    // Post a worker task.
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ImageController::ProcessNextImageDecodeOnWorkerThread,
                   base::Unretained(this)));
  }

  return id;
}

void ImageController::UnlockImageDecode(ImageDecodeRequestId id) {
  // If the image exists, ie we actually need to unlock it, then do so.
  auto it = requested_locked_images_.find(id);
  if (it == requested_locked_images_.end())
    return;

  UnrefImages({it->second});
  requested_locked_images_.erase(it);
}

void ImageController::ProcessNextImageDecodeOnWorkerThread() {
  TRACE_EVENT0("cc", "ImageController::ProcessNextImageDecodeOnWorkerThread");
  ImageDecodeRequest decode;
  {
    base::AutoLock hold(lock_);

    // If we don't have any work, abort.
    if (image_decode_queue_.empty() || abort_tasks_)
      return;

    // Take the next request from the queue.
    auto decode_it = image_decode_queue_.begin();
    DCHECK(decode_it != image_decode_queue_.end());
    decode = std::move(decode_it->second);
    image_decode_queue_.erase(decode_it);

    // Notify that the task will need completion. Note that there are two cases
    // where we process this. First, we might complete this task as a response
    // to the posted task below. Second, we might complete it in
    // StopWorkerTasks(). In either case, the task would have already run
    // (either post task happens after running, or the thread was already joined
    // which means the task ran). This means that we can put the decode into
    // |requests_needing_completion_| here before actually running the task.
    requests_needing_completion_[decode.id] = decode;
  }

  // Run the task if we need to run it. If the task state isn't new, then
  // there is another task that is responsible for finishing it and cleaning
  // up (and it already ran); we just need to post a completion callback.
  // Note that the other tasks's completion will also run first, since the
  // requests are ordered. So, when we process this task's completion, we
  // won't actually do anything with the task and simply issue the callback.
  if (decode.task && decode.task->state().IsNew()) {
    decode.task->state().DidSchedule();
    decode.task->state().DidStart();
    decode.task->RunOnWorkerThread();
    decode.task->state().DidFinish();
  }
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ImageController::ImageDecodeCompleted,
                            weak_ptr_factory_.GetWeakPtr(), decode.id));
}

void ImageController::ImageDecodeCompleted(ImageDecodeRequestId id) {
  ImageDecodedCallback callback;
  {
    base::AutoLock hold(lock_);

    auto request_it = requests_needing_completion_.find(id);
    DCHECK(request_it != requests_needing_completion_.end());
    id = request_it->first;
    ImageDecodeRequest& request = request_it->second;

    // If we need to unref this decode, then we have to put it into the locked
    // images vector.
    if (request.need_unref)
      requested_locked_images_[id] = std::move(request.draw_image);

    // If we have a task that isn't completed yet, we need to complete it.
    if (request.task && !request.task->HasCompleted()) {
      request.task->OnTaskCompleted();
      request.task->DidComplete();
    }
    // Finally, save the callback so we can run it without the lock, and erase
    // the request from |requests_needing_completion_|.
    callback = std::move(request.callback);
    requests_needing_completion_.erase(request_it);
  }

  // Post another task to run.
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ImageController::ProcessNextImageDecodeOnWorkerThread,
                 base::Unretained(this)));

  // Finally run the requested callback.
  callback.Run(id);
}

ImageController::ImageDecodeRequest::ImageDecodeRequest() = default;
ImageController::ImageDecodeRequest::ImageDecodeRequest(
    ImageDecodeRequestId id,
    const DrawImage& draw_image,
    const ImageDecodedCallback& callback,
    scoped_refptr<TileTask> task,
    bool need_unref)
    : id(id),
      draw_image(draw_image),
      callback(callback),
      task(std::move(task)),
      need_unref(need_unref) {}
ImageController::ImageDecodeRequest::ImageDecodeRequest(
    ImageDecodeRequest&& other) = default;
ImageController::ImageDecodeRequest::ImageDecodeRequest(
    const ImageDecodeRequest& other) = default;
ImageController::ImageDecodeRequest::~ImageDecodeRequest() = default;

ImageController::ImageDecodeRequest& ImageController::ImageDecodeRequest::
operator=(ImageDecodeRequest&& other) = default;
ImageController::ImageDecodeRequest& ImageController::ImageDecodeRequest::
operator=(const ImageDecodeRequest& other) = default;

}  // namespace cc
