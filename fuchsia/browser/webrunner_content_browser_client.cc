// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/browser/webrunner_content_browser_client.h"

#include <utility>

#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "fuchsia/browser/webrunner_browser_main_parts.h"

namespace webrunner {

WebRunnerContentBrowserClient::WebRunnerContentBrowserClient(
    zx::channel context_channel)
    : context_channel_(std::move(context_channel)) {}

WebRunnerContentBrowserClient::~WebRunnerContentBrowserClient() = default;

content::BrowserMainParts*
WebRunnerContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  DCHECK(context_channel_);
  main_parts_ = new WebRunnerBrowserMainParts(std::move(context_channel_));
  return main_parts_;
}

std::string WebRunnerContentBrowserClient::GetUserAgent() const {
  return content::BuildUserAgentFromProduct(
      version_info::GetProductNameAndVersionForUserAgent());
}

}  // namespace webrunner
