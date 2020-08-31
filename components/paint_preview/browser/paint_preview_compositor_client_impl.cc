// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_compositor_client_impl.h"

#include <utility>

#include "base/callback.h"

namespace paint_preview {

namespace {

// These methods bind a callback to a task runner. This simplifies situations
// where a caller provides a callback which should be passed to |compositor_|
// verbatim, but should be run on the caller's task runner rather than
// |compositor_task_runner_|.
//
// Based on the implementation in: chromecast/base/bind_to_task_runner.h

template <typename Sig>
struct BindToTaskRunnerTrampoline;

template <typename... Args>
struct BindToTaskRunnerTrampoline<void(Args...)> {
  static void Run(base::TaskRunner* task_runner,
                  base::OnceCallback<void(Args...)> callback,
                  Args... args) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::forward<Args>(args)...));
  }
};

template <typename T>
base::OnceCallback<T> BindToTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner,
    base::OnceCallback<T> callback) {
  return base::BindOnce(&BindToTaskRunnerTrampoline<T>::Run,
                        base::RetainedRef(std::move(task_runner)),
                        std::move(callback));
}

}  // namespace

PaintPreviewCompositorClientImpl::PaintPreviewCompositorClientImpl(
    scoped_refptr<base::SequencedTaskRunner> compositor_task_runner,
    base::WeakPtr<PaintPreviewCompositorServiceImpl> service)
    : compositor_task_runner_(compositor_task_runner),
      default_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      service_(service),
      compositor_(new mojo::Remote<mojom::PaintPreviewCompositor>(),
                  base::OnTaskRunnerDeleter(compositor_task_runner_)) {}

PaintPreviewCompositorClientImpl::~PaintPreviewCompositorClientImpl() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  NotifyServiceOfInvalidation();
}

const base::Optional<base::UnguessableToken>&
PaintPreviewCompositorClientImpl::Token() const {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return token_;
}

void PaintPreviewCompositorClientImpl::SetDisconnectHandler(
    base::OnceClosure closure) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  user_disconnect_closure_ = std::move(closure);
}

// For the following methods the use of base::Unretained for |compositor_| and
// things |compositor_| owns is safe as:
// 1. |compositor_| is deleted on the |compositor_task_runner_| after other
//    non-delayed tasks in the current sequence are run.
// 2. New tasks cannot be created that reference |compositor_| once it is
//    deleted as its lifetime is tied to that of the
//    PaintPreviewCompositorClient.
//
// NOTE: This is only safe as no delayed tasks are posted and there are no
// cases of base::Unretained(this) or other class members passed as pointers.

void PaintPreviewCompositorClientImpl::BeginComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    mojom::PaintPreviewCompositor::BeginCompositeCallback callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojom::PaintPreviewCompositor::BeginComposite,
          base::Unretained(compositor_.get()->get()), std::move(request),
          BindToTaskRunner(default_task_runner_, std::move(callback))));
}

void PaintPreviewCompositorClientImpl::BitmapForFrame(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    mojom::PaintPreviewCompositor::BitmapForFrameCallback callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&mojom::PaintPreviewCompositor::BitmapForFrame,
                                base::Unretained(compositor_.get()->get()),
                                frame_guid, clip_rect, scale_factor,
                                BindToTaskRunner(default_task_runner_,
                                                 std::move(callback))));
}

void PaintPreviewCompositorClientImpl::SetRootFrameUrl(const GURL& url) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&mojom::PaintPreviewCompositor::SetRootFrameUrl,
                     base::Unretained(compositor_.get()->get()), url));
}

void PaintPreviewCompositorClientImpl::IsBoundAndConnected(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  compositor_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](mojo::Remote<mojom::PaintPreviewCompositor>* compositor,
                        scoped_refptr<base::SequencedTaskRunner> task_runner,
                        base::OnceCallback<void(bool)> callback) {
                       task_runner->PostTask(
                           FROM_HERE,
                           base::BindOnce(std::move(callback),
                                          compositor->is_bound() &&
                                              compositor->is_connected()));
                     },
                     base::Unretained(compositor_.get()), default_task_runner_,
                     std::move(callback)));
}

mojo::PendingReceiver<mojom::PaintPreviewCompositor>
PaintPreviewCompositorClientImpl::BindNewPipeAndPassReceiver() {
  DCHECK(compositor_task_runner_->RunsTasksInCurrentSequence());
  return compositor_->BindNewPipeAndPassReceiver();
}

PaintPreviewCompositorClientImpl::OnCompositorCreatedCallback
PaintPreviewCompositorClientImpl::BuildCompositorCreatedCallback(
    base::OnceClosure user_closure,
    OnCompositorCreatedCallback service_callback) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  return BindToTaskRunner(
      default_task_runner_,
      base::BindOnce(&PaintPreviewCompositorClientImpl::OnCompositorCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(user_closure),
                     std::move(service_callback)));
}

void PaintPreviewCompositorClientImpl::OnCompositorCreated(
    base::OnceClosure user_closure,
    OnCompositorCreatedCallback service_callback,
    const base::UnguessableToken& token) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  token_ = token;
  std::move(user_closure).Run();
  std::move(service_callback).Run(token);
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojo::Remote<mojom::PaintPreviewCompositor>::set_disconnect_handler,
          base::Unretained(compositor_.get()),
          BindToTaskRunner(
              default_task_runner_,
              base::BindOnce(
                  &PaintPreviewCompositorClientImpl::DisconnectHandler,
                  weak_ptr_factory_.GetWeakPtr()))));
}

void PaintPreviewCompositorClientImpl::NotifyServiceOfInvalidation() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  if (service_ && token_.has_value())
    service_->MarkCompositorAsDeleted(token_.value());
}

void PaintPreviewCompositorClientImpl::DisconnectHandler() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  if (user_disconnect_closure_)
    std::move(user_disconnect_closure_).Run();
  NotifyServiceOfInvalidation();
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&mojo::Remote<mojom::PaintPreviewCompositor>::reset,
                     base::Unretained(compositor_.get())));
}

}  // namespace paint_preview
