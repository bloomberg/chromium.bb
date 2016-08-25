// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/delegated_output_surface.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cc/output/compositor_frame.h"

namespace blimp {
namespace client {

DelegatedOutputSurface::DelegatedOutputSurface(
    scoped_refptr<cc::ContextProvider> compositor_context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::WeakPtr<BlimpOutputSurfaceClient> client)
    : cc::OutputSurface(std::move(compositor_context_provider),
                        std::move(worker_context_provider),
                        nullptr),
      main_task_runner_(std::move(main_task_runner)),
      client_(client),
      bound_to_client_(false),
      weak_factory_(this) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  capabilities_.delegated_rendering = true;
}

DelegatedOutputSurface::~DelegatedOutputSurface() = default;

void DelegatedOutputSurface::ReclaimCompositorResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  cc::OutputSurface::ReclaimResources(resources);
}

uint32_t DelegatedOutputSurface::GetFramebufferCopyTextureFormat() {
  NOTREACHED() << "Should not be called on delegated output surface";
  return 0;
}

bool DelegatedOutputSurface::BindToClient(cc::OutputSurfaceClient* client) {
  bool success = cc::OutputSurface::BindToClient(client);
  if (success) {
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BlimpOutputSurfaceClient::BindToOutputSurface,
                              client_, weak_factory_.GetWeakPtr()));
  }
  return success;
}

void DelegatedOutputSurface::SwapBuffers(cc::CompositorFrame frame) {
  DCHECK(client_thread_checker_.CalledOnValidThread());

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&BlimpOutputSurfaceClient::SwapCompositorFrame,
                            client_, base::Passed(&frame)));
  cc::OutputSurface::PostSwapBuffersComplete();
}

void DelegatedOutputSurface::DetachFromClient() {
  cc::OutputSurface::DetachFromClient();

  if (bound_to_client_ == true) {
    bound_to_client_ = false;
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BlimpOutputSurfaceClient::UnbindOutputSurface, client_));
  }

  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace client
}  // namespace blimp
