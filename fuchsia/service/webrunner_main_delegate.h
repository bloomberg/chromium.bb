// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_SERVICE_WEBRUNNER_MAIN_DELEGATE_H_
#define FUCHSIA_SERVICE_WEBRUNNER_MAIN_DELEGATE_H_

#include <lib/zx/channel.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/app/content_main_delegate.h"
#include "fuchsia/common/fuchsia_export.h"

namespace content {
class ContentClient;
}  // namespace content

namespace webrunner {

class WebRunnerContentBrowserClient;
class WebRunnerContentRendererClient;

class FUCHSIA_EXPORT WebRunnerMainDelegate
    : public content::ContentMainDelegate {
 public:
  explicit WebRunnerMainDelegate(zx::channel context_channel);
  ~WebRunnerMainDelegate() override;

  static WebRunnerMainDelegate* GetInstanceForTest();

  WebRunnerContentBrowserClient* browser_client() {
    return browser_client_.get();
  }

  // ContentMainDelegate implementation.
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<WebRunnerContentBrowserClient> browser_client_;
  std::unique_ptr<WebRunnerContentRendererClient> renderer_client_;

  zx::channel context_channel_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerMainDelegate);
};

}  // namespace webrunner

#endif  // FUCHSIA_SERVICE_WEBRUNNER_MAIN_DELEGATE_H_
