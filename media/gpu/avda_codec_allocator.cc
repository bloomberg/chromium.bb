// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_codec_allocator.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/limits.h"
#include "media/base/media.h"
#include "media/base/timestamp_constants.h"
#include "media/gpu/android_video_decode_accelerator.h"

namespace media {

namespace {

base::LazyInstance<AVDACodecAllocator>::Leaky g_avda_codec_allocator =
    LAZY_INSTANCE_INITIALIZER;

// Give tasks 800ms before considering them hung. MediaCodec.configure() calls
// typically take 100-200ms on a N5, so 800ms is expected to very rarely result
// in false positives. Also, false positives have low impact because we resume
// using the thread when the task completes.
constexpr base::TimeDelta kHungTaskDetectionTimeout =
    base::TimeDelta::FromMilliseconds(800);

// Delete |codec| and signal |done_event| if it's not null.
void DeleteMediaCodecAndSignal(std::unique_ptr<VideoCodecBridge> codec,
                               base::WaitableEvent* done_event) {
  codec.reset();
  if (done_event)
    done_event->Signal();
}

}  // namespace

CodecConfig::CodecConfig() {}
CodecConfig::~CodecConfig() {}

AVDACodecAllocator::HangDetector::HangDetector(base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

void AVDACodecAllocator::HangDetector::WillProcessTask(
    const base::PendingTask& pending_task) {
  base::AutoLock l(lock_);
  task_start_time_ = tick_clock_->NowTicks();
}

void AVDACodecAllocator::HangDetector::DidProcessTask(
    const base::PendingTask& pending_task) {
  base::AutoLock l(lock_);
  task_start_time_ = base::TimeTicks();
}

bool AVDACodecAllocator::HangDetector::IsThreadLikelyHung() {
  base::AutoLock l(lock_);
  if (task_start_time_.is_null())
    return false;

  return (tick_clock_->NowTicks() - task_start_time_) >
         kHungTaskDetectionTimeout;
}

// static
AVDACodecAllocator* AVDACodecAllocator::Instance() {
  return g_avda_codec_allocator.Pointer();
}

bool AVDACodecAllocator::StartThread(AVDACodecAllocatorClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Cancel any pending StopThreadTask()s because we need the threads now.
  weak_this_factory_.InvalidateWeakPtrs();

  // Try to start the threads if they haven't been started.
  for (auto* thread : threads_) {
    if (thread->thread.IsRunning())
      continue;

    if (!thread->thread.Start())
      return false;

    // Register the hang detector to observe the thread's MessageLoop.
    thread->thread.task_runner()->PostTask(
        FROM_HERE, base::Bind(&base::MessageLoop::AddTaskObserver,
                              base::Unretained(thread->thread.message_loop()),
                              &thread->hang_detector));
  }

  clients_.insert(client);
  return true;
}

void AVDACodecAllocator::StopThread(AVDACodecAllocatorClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  clients_.erase(client);
  if (!clients_.empty()) {
    // If we aren't stopping, then signal immediately.
    if (stop_event_for_testing_)
      stop_event_for_testing_->Signal();
    return;
  }

  // Post a task to stop each thread through its task runner and back to this
  // thread. This ensures that all pending tasks are run first. If a new AVDA
  // calls StartThread() before StopThreadTask() runs, it's canceled by
  // invalidating its weak pointer. As a result we're guaranteed to only call
  // Thread::Stop() while there are no tasks on its queue. We don't try to stop
  // hung threads. But if it recovers it will be stopped the next time a client
  // calls this.
  for (size_t i = 0; i < threads_.size(); i++) {
    if (threads_[i]->thread.IsRunning() &&
        !threads_[i]->hang_detector.IsThreadLikelyHung()) {
      threads_[i]->thread.task_runner()->PostTaskAndReply(
          FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(&AVDACodecAllocator::StopThreadTask,
                     weak_this_factory_.GetWeakPtr(), i));
    }
  }
}

// Return the task runner for tasks of type |type|.
scoped_refptr<base::SingleThreadTaskRunner> AVDACodecAllocator::TaskRunnerFor(
    TaskType task_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return threads_[task_type]->thread.task_runner();
}

bool AVDACodecAllocator::AllocateSurface(AVDACodecAllocatorClient* client,
                                         int surface_id) {
  DVLOG(1) << __func__ << ": " << surface_id;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (surface_id == SurfaceManager::kNoSurfaceID)
    return true;

  // If it's not owned or being released, |client| now owns it.
  if (!surface_owners_.count(surface_id) &&
      !pending_codec_releases_.count(surface_id)) {
    surface_owners_[surface_id].owner = client;
    return true;
  }

  // Otherwise |client| replaces the previous waiter (if any).
  OwnerRecord& record = surface_owners_[surface_id];
  if (record.waiter)
    record.waiter->OnSurfaceAvailable(false);
  record.waiter = client;
  return false;
}

void AVDACodecAllocator::DeallocateSurface(AVDACodecAllocatorClient* client,
                                           int surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (surface_id == SurfaceManager::kNoSurfaceID ||
      !surface_owners_.count(surface_id)) {
    return;
  }

  OwnerRecord& record = surface_owners_[surface_id];
  if (record.owner == client)
    record.owner = nullptr;
  else if (record.waiter == client)
    record.waiter = nullptr;

  // Promote the waiter if possible.
  if (record.waiter && !record.owner &&
      !pending_codec_releases_.count(surface_id)) {
    record.owner = record.waiter;
    record.waiter = nullptr;
    record.owner->OnSurfaceAvailable(true);
    return;
  }

  // Remove the record if it's now unused.
  if (!record.owner && !record.waiter)
    surface_owners_.erase(surface_id);
}

// During surface teardown we have to handle the following cases.
// 1) No AVDA has acquired the surface, or the surface has already been
//    completely released.
// 2) A MediaCodec is currently being configured with the surface on another
//    thread. Whether an AVDA owns the surface or has already deallocated it,
//    the MediaCodec should be dropped when configuration completes.
// 3) An AVDA owns the surface and it responds to OnSurfaceDestroyed() by:
//     a) Replacing the destroyed surface by calling MediaCodec#setSurface().
//     b) Releasing the MediaCodec it's attached to.
// 4) No AVDA owns the surface, but the MediaCodec it's attached to is currently
//    being destroyed on another thread.
void AVDACodecAllocator::OnSurfaceDestroyed(int surface_id) {
  DVLOG(1) << __func__ << ": " << surface_id;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Notify the owner and waiter (if any).
  if (surface_owners_.count(surface_id)) {
    OwnerRecord& record = surface_owners_[surface_id];
    if (record.waiter) {
      record.waiter->OnSurfaceAvailable(false);
      record.waiter = nullptr;
    }

    if (record.owner)
      record.owner->OnSurfaceDestroyed();

    surface_owners_.erase(surface_id);
  }

  // The codec might have been released above in OnSurfaceDestroyed(), or was
  // already pending release.
  if (!pending_codec_releases_.count(surface_id))
    return;

  // The codec is being released so we have to wait for it here. It's a
  // TimedWait() because the MediaCodec release may hang due to framework bugs.
  // And in that case we don't want to hang the browser UI thread. Android ANRs
  // occur when the UI thread is blocked for 5 seconds, so waiting for 2 seconds
  // gives us leeway to avoid an ANR. Verified no ANR on a Nexus 7.
  base::WaitableEvent& released =
      pending_codec_releases_.find(surface_id)->second;
  released.TimedWait(base::TimeDelta::FromSeconds(2));
  if (!released.IsSignaled())
    DLOG(WARNING) << __func__ << ": timed out waiting for MediaCodec#release()";
}

std::unique_ptr<VideoCodecBridge> AVDACodecAllocator::CreateMediaCodecSync(
    scoped_refptr<CodecConfig> codec_config) {
  TRACE_EVENT0("media", "AVDA::CreateMediaCodecSync");

  jobject media_crypto = codec_config->media_crypto_
                             ? codec_config->media_crypto_->obj()
                             : nullptr;

  // |needs_protected_surface_| implies encrypted stream.
  DCHECK(!codec_config->needs_protected_surface_ || media_crypto);

  const bool require_software_codec = codec_config->task_type_ == SW_CODEC;

  std::unique_ptr<VideoCodecBridge> codec(VideoCodecBridge::CreateDecoder(
      codec_config->codec_, codec_config->needs_protected_surface_,
      codec_config->initial_expected_coded_size_,
      codec_config->surface_.j_surface().obj(), media_crypto,
      codec_config->csd0_, codec_config->csd1_, true, require_software_codec));

  return codec;
}

void AVDACodecAllocator::CreateMediaCodecAsync(
    base::WeakPtr<AVDACodecAllocatorClient> client,
    scoped_refptr<CodecConfig> codec_config) {
  base::PostTaskAndReplyWithResult(
      TaskRunnerFor(codec_config->task_type_).get(), FROM_HERE,
      base::Bind(&AVDACodecAllocator::CreateMediaCodecSync,
                 base::Unretained(this), codec_config),
      base::Bind(&AVDACodecAllocatorClient::OnCodecConfigured, client));
}

void AVDACodecAllocator::ReleaseMediaCodec(
    std::unique_ptr<VideoCodecBridge> media_codec,
    TaskType task_type,
    int surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(media_codec);

  // No need to track the release if it's a SurfaceTexture.
  if (surface_id == SurfaceManager::kNoSurfaceID) {
    TaskRunnerFor(task_type)->PostTask(
        FROM_HERE, base::Bind(&DeleteMediaCodecAndSignal,
                              base::Passed(std::move(media_codec)), nullptr));
    return;
  }

  pending_codec_releases_.emplace(
      std::piecewise_construct, std::forward_as_tuple(surface_id),
      std::forward_as_tuple(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED));
  base::WaitableEvent* released =
      &pending_codec_releases_.find(surface_id)->second;

  TaskRunnerFor(task_type)->PostTaskAndReply(
      FROM_HERE, base::Bind(&DeleteMediaCodecAndSignal,
                            base::Passed(std::move(media_codec)), released),
      base::Bind(&AVDACodecAllocator::OnMediaCodecAndSurfaceReleased,
                 base::Unretained(this), surface_id));
}

void AVDACodecAllocator::OnMediaCodecAndSurfaceReleased(int surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  pending_codec_releases_.erase(surface_id);
  if (!surface_owners_.count(surface_id))
    return;

  OwnerRecord& record = surface_owners_[surface_id];
  if (!record.owner && record.waiter) {
    record.owner = record.waiter;
    record.waiter = nullptr;
    record.owner->OnSurfaceAvailable(true);
  }
}

// Returns a hint about whether the construction thread has hung for
// |task_type|.  Note that if a thread isn't started, then we'll just return
// "not hung", since it'll run on the current thread anyway.  The hang
// detector will see no pending jobs in that case, so it's automatic.
bool AVDACodecAllocator::IsThreadLikelyHung(TaskType task_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return threads_[task_type]->hang_detector.IsThreadLikelyHung();
}

bool AVDACodecAllocator::IsAnyRegisteredAVDA() {
  return !clients_.empty();
}

base::Optional<TaskType> AVDACodecAllocator::TaskTypeForAllocation() {
  if (!IsThreadLikelyHung(AUTO_CODEC))
    return AUTO_CODEC;

  if (!IsThreadLikelyHung(SW_CODEC))
    return SW_CODEC;

  return base::nullopt;
}

base::Thread& AVDACodecAllocator::GetThreadForTesting(TaskType task_type) {
  return threads_[task_type]->thread;
}

AVDACodecAllocator::AVDACodecAllocator(base::TickClock* tick_clock,
                                       base::WaitableEvent* stop_event)
    : stop_event_for_testing_(stop_event), weak_this_factory_(this) {
  // We leak the clock we create, but that's okay because we're a singleton.
  auto clock = tick_clock ? tick_clock : new base::DefaultTickClock();

  // Create threads with names and indices that match up with TaskType.
  threads_.push_back(new ThreadAndHangDetector("AVDAAutoThread", clock));
  threads_.push_back(new ThreadAndHangDetector("AVDASWThread", clock));
  static_assert(AUTO_CODEC == 0 && SW_CODEC == 1,
                "TaskType values are not ordered correctly.");
}

AVDACodecAllocator::~AVDACodecAllocator() {
  // Only tests should reach here.  Shut down threads so that we guarantee that
  // nothing will use the threads.
  for (auto* thread : threads_)
    thread->thread.Stop();
}

void AVDACodecAllocator::StopThreadTask(size_t index) {
  threads_[index]->thread.Stop();
  // Signal the stop event after both threads are stopped.
  if (stop_event_for_testing_ && !threads_[AUTO_CODEC]->thread.IsRunning() &&
      !threads_[SW_CODEC]->thread.IsRunning()) {
    stop_event_for_testing_->Signal();
  }
}

}  // namespace media
