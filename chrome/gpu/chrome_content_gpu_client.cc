// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_CHROMEOS)
#include "chrome/gpu/gpu_arc_video_decode_accelerator.h"
#include "chrome/gpu/gpu_arc_video_encode_accelerator.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#endif

ChromeContentGpuClient::ChromeContentGpuClient() {}
ChromeContentGpuClient::~ChromeContentGpuClient() {}

void ChromeContentGpuClient::InitializeRegistry(
    service_manager::BinderRegistry* registry) {
#if defined(OS_CHROMEOS)
  registry->AddInterface(
      base::Bind(&ChromeContentGpuClient::CreateArcVideoDecodeAccelerator,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface(
      base::Bind(&ChromeContentGpuClient::CreateArcVideoEncodeAccelerator,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());
#endif
}

void ChromeContentGpuClient::GpuServiceInitialized(
    const gpu::GpuPreferences& gpu_preferences) {
#if defined(OS_CHROMEOS)
  gpu_preferences_ = gpu_preferences;
#endif

  // Note: The call stack profile collection is initiated in ChromeMain.
  metrics::mojom::CallStackProfileCollectorPtr browser_interface;
  content::ChildThread::Get()->GetConnector()->BindInterface(
      content::mojom::kBrowserServiceName, &browser_interface);
  metrics::ChildCallStackProfileCollector::Get()->SetParentProfileCollector(
      std::move(browser_interface));
}

#if defined(OS_CHROMEOS)

void ChromeContentGpuClient::CreateArcVideoDecodeAccelerator(
    ::arc::mojom::VideoDecodeAcceleratorRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<chromeos::arc::GpuArcVideoDecodeAccelerator>(
          gpu_preferences_),
      std::move(request));
}

void ChromeContentGpuClient::CreateArcVideoEncodeAccelerator(
    ::arc::mojom::VideoEncodeAcceleratorRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<chromeos::arc::GpuArcVideoEncodeAccelerator>(
          gpu_preferences_),
      std::move(request));
}
#endif
