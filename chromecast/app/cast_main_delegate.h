// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_CAST_MAIN_DELEGATE_H_
#define CHROMECAST_APP_CAST_MAIN_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "chromecast/common/cast_content_client.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class BrowserMainRunner;
}  // namespace content

namespace chromecast {

class CastResourceDelegate;

namespace shell {

class CastContentBrowserClient;
class CastContentRendererClient;

class CastMainDelegate : public content::ContentMainDelegate {
 public:
  CastMainDelegate();
  ~CastMainDelegate() override;

  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
#if !defined(OS_ANDROID)
  void ZygoteForked() override;
#endif  // !defined(OS_ANDROID)
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  void InitializeResourceBundle();

  scoped_ptr<CastContentBrowserClient> browser_client_;
  scoped_ptr<CastContentRendererClient> renderer_client_;
  scoped_ptr<CastResourceDelegate> resource_delegate_;
  CastContentClient content_client_;

#if defined(OS_ANDROID)
  scoped_ptr<content::BrowserMainRunner> browser_runner_;
#endif  // defined(OS_ANDROID)

  DISALLOW_COPY_AND_ASSIGN(CastMainDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_APP_CAST_MAIN_DELEGATE_H_
