// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_DELEGATE_H_
#define CHROME_APP_CHROME_MAIN_DELEGATE_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/metrics/stats_counters.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/app/content_main_delegate.h"

// Chrome implementation of ContentMainDelegate.
class ChromeMainDelegate : public content::ContentMainDelegate {
 public:
  ChromeMainDelegate();
  virtual ~ChromeMainDelegate();

  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;

#if defined(OS_MACOSX)
  void InitMacCrashReporter(const CommandLine& command_line,
                            const std::string& process_type);
#endif  // defined(OS_MACOSX)

  virtual void PreSandboxStartup() OVERRIDE;
  virtual void SandboxInitialized(const std::string& process_type) OVERRIDE;
  virtual int RunProcess(
      const std::string& process_type,
      const MainFunctionParams& main_function_params) OVERRIDE;
  virtual void ProcessExiting(const std::string& process_type) OVERRIDE;

#if defined(OS_MACOSX)
  virtual bool ProcessRegistersWithSystemProcess(
      const std::string& process_type) OVERRIDE;
  virtual bool ShouldSendMachPort(const std::string& process_type) OVERRIDE;
  virtual bool DelaySandboxInitialization(
      const std::string& process_type) OVERRIDE;
#elif defined(OS_POSIX)
  virtual ZygoteForkDelegate* ZygoteStarting() OVERRIDE;
  virtual void ZygoteForked() OVERRIDE;
#endif

 private:
  chrome::ChromeContentClient chrome_content_client_;
  scoped_ptr<base::StatsScope<base::StatsCounterTimer> > startup_timer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegate);
};

#endif  // CHROME_APP_CHROME_MAIN_DELEGATE_H_
