// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/service_registry.h"
#include "content/renderer/media/media_renderer_service_provider.h"

namespace content {

MediaRendererServiceProvider::MediaRendererServiceProvider(
    ServiceRegistry* service_registry)
    : service_registry_(service_registry) {
}

MediaRendererServiceProvider::~MediaRendererServiceProvider() {
}

void MediaRendererServiceProvider::ConnectToService(
    mojo::InterfacePtr<mojo::MediaRenderer>* media_renderer_ptr) {
  service_registry_->ConnectToRemoteService(media_renderer_ptr);
}

}  // namespace content
