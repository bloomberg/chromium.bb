// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BLIMP_CHROME_COMPOSITOR_DEPENDENCIES_H_
#define CHROME_BROWSER_ANDROID_BLIMP_CHROME_COMPOSITOR_DEPENDENCIES_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "ui/android/context_provider_factory.h"

// A wrapper for the Blimp compositor dependencies that passes through to the
// ui::ContextProviderFactory. The ContextProviderFactory must outlive this
// class.
class ChromeCompositorDependencies
    : public blimp::client::CompositorDependencies {
 public:
  ChromeCompositorDependencies(
      ui::ContextProviderFactory* context_provider_factory);
  ~ChromeCompositorDependencies() override;

  // CompositorDependencies implementation.
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::SurfaceManager* GetSurfaceManager() override;
  cc::FrameSinkId AllocateFrameSinkId() override;
  void GetContextProviders(const ContextProviderCallback& callback) override;

 private:
  void OnGpuChannelEstablished(
      const ContextProviderCallback& callback,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      ui::ContextProviderFactory::GpuChannelHostResult result);

  void HandlePendingRequests(gpu::GpuChannelHost* gpu_channel_host);

  ui::ContextProviderFactory* context_provider_factory_;

  // Worker context shared across all Blimp Compositors.
  scoped_refptr<cc::ContextProvider> shared_main_thread_worker_context_;

  base::WeakPtrFactory<ChromeCompositorDependencies> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCompositorDependencies);
};

#endif  // CHROME_BROWSER_ANDROID_BLIMP_CHROME_COMPOSITOR_DEPENDENCIES_H_
