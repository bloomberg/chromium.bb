// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_APP_CAST_MAIN_DELEGATE_H_
#define CHROMECAST_SHELL_APP_CAST_MAIN_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chromecast/shell/common/cast_content_client.h"
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
  virtual ~CastMainDelegate();

  // content::ContentMainDelegate implementation:
  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;
  virtual void PreSandboxStartup() OVERRIDE;
  virtual int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) OVERRIDE;
#if !defined(OS_ANDROID)
  virtual void ZygoteForked() OVERRIDE;
#endif  // !defined(OS_ANDROID)
  virtual content::ContentBrowserClient* CreateContentBrowserClient() OVERRIDE;
  virtual content::ContentRendererClient*
      CreateContentRendererClient() OVERRIDE;

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

#endif  // CHROMECAST_SHELL_APP_CAST_MAIN_DELEGATE_H_
