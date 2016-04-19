// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_DELEGATE_H_
#define CHROME_APP_CHROME_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/app/content_main_delegate.h"

template <typename>
class ScopedVector;

namespace base {
class CommandLine;
}

// Chrome implementation of ContentMainDelegate.
class ChromeMainDelegate : public content::ContentMainDelegate {
 public:
  ChromeMainDelegate();
  ~ChromeMainDelegate() override;

 protected:
  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  void SandboxInitialized(const std::string& process_type) override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
#if defined(OS_MACOSX)
  bool ProcessRegistersWithSystemProcess(
      const std::string& process_type) override;
  bool ShouldSendMachPort(const std::string& process_type) override;
  bool DelaySandboxInitialization(const std::string& process_type) override;
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  void ZygoteStarting(
      ScopedVector<content::ZygoteForkDelegate>* delegates) override;
  void ZygoteForked() override;
#endif
  bool ShouldEnableProfilerRecording() override;

  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

#if defined(OS_MACOSX)
  void InitMacCrashReporter(const base::CommandLine& command_line,
                            const std::string& process_type);
#endif  // defined(OS_MACOSX)

  ChromeContentClient chrome_content_client_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegate);
};

#endif  // CHROME_APP_CHROME_MAIN_DELEGATE_H_
