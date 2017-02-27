// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
#define HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/app/content_main_delegate.h"
#include "headless/lib/browser/headless_platform_event_source.h"
#include "headless/lib/headless_content_client.h"

namespace base {
class CommandLine;
}

namespace headless {

class HeadlessBrowserImpl;
class HeadlessContentBrowserClient;

class HeadlessContentMainDelegate : public content::ContentMainDelegate {
 public:
  explicit HeadlessContentMainDelegate(
      std::unique_ptr<HeadlessBrowserImpl> browser);
  ~HeadlessContentMainDelegate() override;

  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;

  HeadlessBrowserImpl* browser() const { return browser_.get(); }

#if !defined(OS_MACOSX) && defined(OS_POSIX) && !defined(OS_ANDROID)
  void ZygoteForked() override;
#endif

 private:
  friend class HeadlessBrowserTest;

  void InitLogging(const base::CommandLine& command_line);
  void InitCrashReporter(const base::CommandLine& command_line);
  static void InitializeResourceBundle();

  static HeadlessContentMainDelegate* GetInstance();

  std::unique_ptr<HeadlessContentBrowserClient> browser_client_;
  HeadlessContentClient content_client_;
  HeadlessPlatformEventSource platform_event_source_;

  std::unique_ptr<HeadlessBrowserImpl> browser_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessContentMainDelegate);
};

}  // namespace headless

#endif  // HEADLESS_LIB_HEADLESS_CONTENT_MAIN_DELEGATE_H_
