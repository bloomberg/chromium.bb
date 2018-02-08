// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_url_loader_throttle_provider.h"

#include <memory>

#include "base/feature_list.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/renderer/renderer_url_loader_throttle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"

namespace android_webview {

AwURLLoaderThrottleProvider::AwURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType type)
    : type_(type) {
  DETACH_FROM_THREAD(thread_checker_);

  if (base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      base::FeatureList::IsEnabled(safe_browsing::kCheckByURLLoaderThrottle)) {
    content::RenderThread::Get()->GetConnector()->BindInterface(
        content::mojom::kBrowserServiceName,
        mojo::MakeRequest(&safe_browsing_info_));
  }
}

AwURLLoaderThrottleProvider::~AwURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::vector<std::unique_ptr<content::URLLoaderThrottle>>
AwURLLoaderThrottleProvider::CreateThrottles(
    int render_frame_id,
    const blink::WebURL& url,
    content::ResourceType resource_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<std::unique_ptr<content::URLLoaderThrottle>> throttles;

  bool network_service_enabled =
      base::FeatureList::IsEnabled(network::features::kNetworkService);
  // Some throttles have already been added in the browser for frame resources.
  // Don't add them for frame requests.
  bool is_frame_resource = content::IsResourceTypeFrame(resource_type);

  DCHECK(!is_frame_resource ||
         type_ == content::URLLoaderThrottleProviderType::kFrame);

  if ((network_service_enabled ||
       base::FeatureList::IsEnabled(
           safe_browsing::kCheckByURLLoaderThrottle)) &&
      !is_frame_resource) {
    if (safe_browsing_info_)
      safe_browsing_.Bind(std::move(safe_browsing_info_));
    throttles.push_back(
        std::make_unique<safe_browsing::RendererURLLoaderThrottle>(
            safe_browsing_.get(), render_frame_id));
  }

  return throttles;
}

}  // namespace android_webview
