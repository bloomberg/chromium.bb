// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include "base/command_line.h"
#include "services/shell/public/cpp/interface_registry.h"

#if defined(OS_CHROMEOS)
#include "chrome/gpu/gpu_arc_video_service.h"
#endif

#if defined(OS_CHROMEOS)
namespace {

void DeprecatedCreateGpuArcVideoService(
    ::arc::mojom::VideoAcceleratorServiceClientRequest request) {
  // GpuArcVideoService is strongly bound to the Mojo message pipe it
  // is connected to. When that message pipe is closed, either explicitly on the
  // other end (in the browser process), or by a connection error, this object
  // will be destroyed.
  auto* service = new chromeos::arc::GpuArcVideoService();
  service->Connect(std::move(request));
}

void CreateGpuArcVideoService(
    ::arc::mojom::VideoAcceleratorServiceRequest request) {
  // GpuArcVideoService is strongly bound to the Mojo message pipe it
  // is connected to. When that message pipe is closed, either explicitly on the
  // other end (in the browser process), or by a connection error, this object
  // will be destroyed.
  new chromeos::arc::GpuArcVideoService(std::move(request));
}

}  // namespace
#endif

ChromeContentGpuClient::ChromeContentGpuClient() {}

ChromeContentGpuClient::~ChromeContentGpuClient() {}

void ChromeContentGpuClient::ExposeInterfacesToBrowser(
    shell::InterfaceRegistry* registry) {
#if defined(OS_CHROMEOS)
  registry->AddInterface(base::Bind(&CreateGpuArcVideoService));
  registry->AddInterface(base::Bind(&DeprecatedCreateGpuArcVideoService));
#endif
}

void ChromeContentGpuClient::Initialize(
    base::FieldTrialList::Observer* observer) {
  DCHECK(!field_trial_syncer_);
  field_trial_syncer_.reset(
      new chrome_variations::ChildProcessFieldTrialSyncer(observer));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  field_trial_syncer_->InitFieldTrialObserving(command_line);
}
