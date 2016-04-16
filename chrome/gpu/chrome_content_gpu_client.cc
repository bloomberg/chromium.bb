// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include "content/public/common/service_registry.h"

#if defined(OS_CHROMEOS)
#include "chrome/gpu/gpu_arc_video_service.h"
#endif

#if defined(OS_CHROMEOS)
namespace {

void CreateGpuArcVideoService(
    mojo::InterfaceRequest<::arc::VideoHost> request) {
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

void ChromeContentGpuClient::RegisterMojoServices(
    content::ServiceRegistry* registry) {
#if defined(OS_CHROMEOS)
  registry->AddService(base::Bind(&CreateGpuArcVideoService));
#endif
}
