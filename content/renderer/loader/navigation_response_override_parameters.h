// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_NAVIGATION_RESPONSE_OVERRIDE_PARAMETERS_H_
#define CONTENT_RENDERER_LOADER_NAVIGATION_RESPONSE_OVERRIDE_PARAMETERS_H_

#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace content {

// PlzNavigate: Used to override parameters of the navigation request.
struct CONTENT_EXPORT NavigationResponseOverrideParameters {
 public:
  NavigationResponseOverrideParameters();
  ~NavigationResponseOverrideParameters();

  GURL stream_url;
  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;
  network::ResourceResponseHead response;
  std::vector<GURL> redirects;
  std::vector<network::ResourceResponseHead> redirect_responses;
  std::vector<net::RedirectInfo> redirect_infos;

  // Called when this struct is deleted. Used to notify the browser that it can
  // release its associated StreamHandle.
  base::OnceCallback<void(const GURL&)> on_delete;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_NAVIGATION_RESPONSE_OVERRIDE_PARAMETERS_H_