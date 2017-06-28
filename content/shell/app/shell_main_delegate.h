// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class ContentClient;
class ShellContentBrowserClient;
class ShellContentRendererClient;
class ShellContentUtilityClient;

#if defined(OS_ANDROID)
class BrowserMainRunner;
#endif

class ShellMainDelegate : public ContentMainDelegate {
 public:
  ShellMainDelegate();
  ~ShellMainDelegate() override;

  // ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(const std::string& process_type,
                 const MainFunctionParams& main_function_params) override;
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
  void ZygoteForked() override;
#endif
  ContentBrowserClient* CreateContentBrowserClient() override;
  ContentRendererClient* CreateContentRendererClient() override;
  ContentUtilityClient* CreateContentUtilityClient() override;

  static void InitializeResourceBundle();

 private:
  std::unique_ptr<ShellContentBrowserClient> browser_client_;
  std::unique_ptr<ShellContentRendererClient> renderer_client_;
  std::unique_ptr<ShellContentUtilityClient> utility_client_;
  std::unique_ptr<ContentClient> content_client_;

#if defined(OS_ANDROID)
  std::unique_ptr<BrowserMainRunner> browser_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShellMainDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
