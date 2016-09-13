// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_registry.h"

#if defined(OS_CHROMEOS)
#include "chrome/gpu/gpu_arc_video_service.h"
#endif

#if defined(OS_CHROMEOS)
namespace {

void DeprecatedCreateGpuArcVideoService(
    const gpu::GpuPreferences& gpu_preferences,
    ::arc::mojom::VideoAcceleratorServiceClientRequest client_request) {
  chromeos::arc::GpuArcVideoService::DeprecatedConnect(
      base::MakeUnique<chromeos::arc::GpuArcVideoService>(gpu_preferences),
      std::move(client_request));
}

void CreateGpuArcVideoService(
    const gpu::GpuPreferences& gpu_preferences,
    ::arc::mojom::VideoAcceleratorServiceRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<chromeos::arc::GpuArcVideoService>(gpu_preferences),
      std::move(request));
}

}  // namespace
#endif

ChromeContentGpuClient::ChromeContentGpuClient() {}

ChromeContentGpuClient::~ChromeContentGpuClient() {}

void ChromeContentGpuClient::Initialize(
    base::FieldTrialList::Observer* observer) {
  DCHECK(!field_trial_syncer_);
  field_trial_syncer_.reset(
      new chrome_variations::ChildProcessFieldTrialSyncer(observer));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  field_trial_syncer_->InitFieldTrialObserving(command_line);
}

void ChromeContentGpuClient::ExposeInterfacesToBrowser(
    shell::InterfaceRegistry* registry,
    const gpu::GpuPreferences& gpu_preferences) {
#if defined(OS_CHROMEOS)
  registry->AddInterface(
      base::Bind(&CreateGpuArcVideoService, gpu_preferences));
  registry->AddInterface(
      base::Bind(&DeprecatedCreateGpuArcVideoService, gpu_preferences));
#endif
}

void ChromeContentGpuClient::ConsumeInterfacesFromBrowser(
    shell::InterfaceProvider* provider) {
}
