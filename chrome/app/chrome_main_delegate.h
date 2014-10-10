// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_DELEGATE_H_
#define CHROME_APP_CHROME_MAIN_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "base/metrics/stats_counters.h"
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
  virtual ~ChromeMainDelegate();

 protected:
  // content::ContentMainDelegate implementation:
  virtual bool BasicStartupComplete(int* exit_code) override;
  virtual void PreSandboxStartup() override;
  virtual void SandboxInitialized(const std::string& process_type) override;
  virtual int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  virtual void ProcessExiting(const std::string& process_type) override;
#if defined(OS_MACOSX)
  virtual bool ProcessRegistersWithSystemProcess(
      const std::string& process_type) override;
  virtual bool ShouldSendMachPort(const std::string& process_type) override;
  virtual bool DelaySandboxInitialization(
      const std::string& process_type) override;
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  virtual void ZygoteStarting(
      ScopedVector<content::ZygoteForkDelegate>* delegates) override;
  virtual void ZygoteForked() override;
#elif defined(OS_WIN)
  virtual bool ShouldEnableTerminationOnHeapCorruption() override;
#endif

  virtual content::ContentBrowserClient* CreateContentBrowserClient() override;
  virtual content::ContentPluginClient* CreateContentPluginClient() override;
  virtual content::ContentRendererClient*
      CreateContentRendererClient() override;
  virtual content::ContentUtilityClient* CreateContentUtilityClient() override;

#if defined(OS_MACOSX)
  void InitMacCrashReporter(const base::CommandLine& command_line,
                            const std::string& process_type);
#endif  // defined(OS_MACOSX)

  ChromeContentClient chrome_content_client_;

  // startup_timer_ will hold a reference to stats_counter_timer_. Therefore,
  // the declaration order of these variables matters. Changing this order will
  // cause startup_timer_ to be freed before stats_counter_timer_, leaving a
  // dangling reference.
  scoped_ptr<base::StatsCounterTimer> stats_counter_timer_;
  scoped_ptr<base::StatsScope<base::StatsCounterTimer> > startup_timer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegate);
};

#endif  // CHROME_APP_CHROME_MAIN_DELEGATE_H_
