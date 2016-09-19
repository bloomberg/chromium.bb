// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor_frame_sink.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"

namespace blimp {
namespace client {

BlimpCompositorFrameSink::BlimpCompositorFrameSink(
    scoped_refptr<cc::ContextProvider> compositor_context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::WeakPtr<BlimpCompositorFrameSinkProxy> main_thread_proxy)
    : cc::CompositorFrameSink(std::move(compositor_context_provider),
                              std::move(worker_context_provider)),
      main_task_runner_(std::move(main_task_runner)),
      main_thread_proxy_(main_thread_proxy),
      bound_to_client_(false),
      weak_factory_(this) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

BlimpCompositorFrameSink::~BlimpCompositorFrameSink() = default;

void BlimpCompositorFrameSink::ReclaimCompositorResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  client_->ReclaimResources(resources);
}

bool BlimpCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* client) {
  bool success = cc::CompositorFrameSink::BindToClient(client);
  if (success) {
    begin_frame_source_ = base::MakeUnique<cc::DelayBasedBeginFrameSource>(
        base::MakeUnique<cc::DelayBasedTimeSource>(
            base::ThreadTaskRunnerHandle::Get().get()));
    client->SetBeginFrameSource(begin_frame_source_.get());

    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BlimpCompositorFrameSinkProxy::BindToProxyClient,
                              main_thread_proxy_, weak_factory_.GetWeakPtr()));
  }
  return success;
}

void BlimpCompositorFrameSink::DetachFromClient() {
  cc::CompositorFrameSink::DetachFromClient();

  if (bound_to_client_ == true) {
    bound_to_client_ = false;
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BlimpCompositorFrameSinkProxy::UnbindProxyClient,
                              main_thread_proxy_));
  }

  weak_factory_.InvalidateWeakPtrs();
}

void BlimpCompositorFrameSink::SwapBuffers(cc::CompositorFrame frame) {
  DCHECK(client_thread_checker_.CalledOnValidThread());

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&BlimpCompositorFrameSinkProxy::SwapCompositorFrame,
                            main_thread_proxy_, base::Passed(&frame)));
  cc::CompositorFrameSink::PostSwapBuffersComplete();
}

}  // namespace client
}  // namespace blimp
