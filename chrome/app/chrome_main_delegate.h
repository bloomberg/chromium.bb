// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_DELEGATE_H_
#define CHROME_APP_CHROME_MAIN_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/app/content_main_delegate.h"

namespace base {
class CommandLine;
}

// Chrome implementation of ContentMainDelegate.
class ChromeMainDelegate : public content::ContentMainDelegate {
 public:
  static const char* const kNonWildcardDomainNonPortSchemes[];
  static const size_t kNonWildcardDomainNonPortSchemesSize;

  ChromeMainDelegate();

  // |exe_entry_point_ticks| is the time at which the main function of the
  // executable was entered, or null if not available.
  explicit ChromeMainDelegate(base::TimeTicks exe_entry_point_ticks);
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
  void ZygoteStarting(std::vector<std::unique_ptr<content::ZygoteForkDelegate>>*
                          delegates) override;
  void ZygoteForked() override;
#endif
  bool ShouldEnableProfilerRecording() override;
  service_manager::ProcessType OverrideProcessType() override;
  bool ShouldTerminateServiceManagerOnInstanceQuit(
      const service_manager::Identity& identity,
      int* exit_code) override;

  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

#if defined(OS_MACOSX)
  void InitMacCrashReporter(const base::CommandLine& command_line,
                            const std::string& process_type);
  void SetUpInstallerPreferences(const base::CommandLine& command_line);
#endif  // defined(OS_MACOSX)

  ChromeContentClient chrome_content_client_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegate);
};

#endif  // CHROME_APP_CHROME_MAIN_DELEGATE_H_
