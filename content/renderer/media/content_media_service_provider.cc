// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/content_media_service_provider.h"

#include "content/public/common/service_registry.h"

namespace content {

ContentMediaServiceProvider::ContentMediaServiceProvider(
    RenderFrame* render_frame,
    ServiceRegistry* service_registry)
    : RenderFrameObserver(render_frame), service_registry_(service_registry) {
  DCHECK(service_registry_);
}

ContentMediaServiceProvider::~ContentMediaServiceProvider() {
}

void ContentMediaServiceProvider::ConnectToService(
    mojo::InterfacePtr<mojo::MediaRenderer>* media_renderer_ptr) {
  service_registry_->ConnectToRemoteService(mojo::GetProxy(media_renderer_ptr));
}

void ContentMediaServiceProvider::ConnectToService(
    mojo::InterfacePtr<mojo::ContentDecryptionModule>* cdm_ptr) {
  service_registry_->ConnectToRemoteService(mojo::GetProxy(cdm_ptr));
}

}  // namespace content
