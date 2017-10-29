// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_

#include "content/common/service_worker/controller_service_worker.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"

namespace content {

// For NetworkService glues:
// Navigation parameters that are necessary to set-up a subresource loader
// for the frame that is going to be created by the navigation.
// Passed from the browser to the renderer when the navigation commits when
// NetworkService or its glue code for relevant features is enabled.
struct CONTENT_EXPORT SubresourceLoaderParams {
  SubresourceLoaderParams();
  ~SubresourceLoaderParams();

  SubresourceLoaderParams(SubresourceLoaderParams&& other);
  SubresourceLoaderParams& operator=(SubresourceLoaderParams&& other);

  // The subresource loader factory info that is to be used to create a
  // subresource loader in the renderer. Used by AppCache and WebUI.
  mojom::URLLoaderFactoryPtrInfo loader_factory_info;

  // TODO(kinuko): Add the controller interface ptr for the service worker.
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
