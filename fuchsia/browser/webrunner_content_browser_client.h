// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BROWSER_WEBRUNNER_CONTENT_BROWSER_CLIENT_H_
#define FUCHSIA_BROWSER_WEBRUNNER_CONTENT_BROWSER_CLIENT_H_

#include <lib/zx/channel.h>

#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"

namespace webrunner {

class WebRunnerBrowserMainParts;

class WebRunnerContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit WebRunnerContentBrowserClient(zx::channel context_channel);
  ~WebRunnerContentBrowserClient() override;

  WebRunnerBrowserMainParts* main_parts_for_test() const { return main_parts_; }

  // ContentBrowserClient overrides.
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  std::string GetUserAgent() const override;

 private:
  zx::channel context_channel_;
  WebRunnerBrowserMainParts* main_parts_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerContentBrowserClient);
};

}  // namespace webrunner

#endif  // FUCHSIA_BROWSER_WEBRUNNER_CONTENT_BROWSER_CLIENT_H_
