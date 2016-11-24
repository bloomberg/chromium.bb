// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
#define CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/variations/child_process_field_trial_syncer.h"
#include "content/public/gpu/content_gpu_client.h"

class ChromeContentGpuClient : public content::ContentGpuClient {
 public:
  ChromeContentGpuClient();
  ~ChromeContentGpuClient() override;

  // content::ContentGpuClient:
  void Initialize(base::FieldTrialList::Observer* observer) override;
  void ExposeInterfacesToBrowser(
      service_manager::InterfaceRegistry* registry,
      const gpu::GpuPreferences& gpu_preferences) override;
  void ConsumeInterfacesFromBrowser(
      service_manager::InterfaceProvider* provider) override;

 private:
  std::unique_ptr<variations::ChildProcessFieldTrialSyncer> field_trial_syncer_;
  // Used to profile process startup.
  base::StackSamplingProfiler stack_sampling_profiler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentGpuClient);
};

#endif  // CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
