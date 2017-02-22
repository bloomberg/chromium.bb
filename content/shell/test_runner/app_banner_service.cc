// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/app_banner_service.h"

namespace test_runner {

AppBannerService::AppBannerService() : binding_(this) {}

AppBannerService::~AppBannerService() {}

void AppBannerService::ResolvePromise(const std::string& platform) {
  if (!event_.is_bound())
    return;

  // Empty platform means to resolve as a dismissal.
  if (platform.empty())
    event_->BannerDismissed();
  else
    event_->BannerAccepted(platform);
}

void AppBannerService::SendBannerPromptRequest(
    const std::vector<std::string>& platforms,
    const base::Callback<void(bool)>& callback) {
  if (!controller_.is_bound())
    return;

  controller_->BannerPromptRequest(
      binding_.CreateInterfacePtrAndBind(), mojo::MakeRequest(&event_),
      platforms, base::Bind(&AppBannerService::OnBannerPromptReply,
                            base::Unretained(this), callback));
}

void AppBannerService::DisplayAppBanner() { /* do nothing */
}

void AppBannerService::OnBannerPromptReply(
    const base::Callback<void(bool)>& callback,
    blink::mojom::AppBannerPromptReply reply,
    const std::string& referrer) {
  callback.Run(reply == blink::mojom::AppBannerPromptReply::CANCEL);
}

}  // namespace test_runner
