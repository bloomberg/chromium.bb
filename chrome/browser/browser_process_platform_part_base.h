// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_BASE_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_BASE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class CommandLine;
}

namespace policy {
class BrowserPolicyConnector;
}

// A base class for platform-specific BrowserProcessPlatformPart
// implementations. This class itself should never be used verbatim.
class BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPartBase();
  virtual ~BrowserProcessPlatformPartBase();

  // Called after creating the process singleton or when another chrome
  // rendez-vous with this one.
  virtual void PlatformSpecificCommandLineProcessing(
      const base::CommandLine& command_line);

  // Called from BrowserProcessImpl::StartTearDown().
  virtual void StartTearDown();

  // Called from AttemptExitInternal().
  virtual void AttemptExit();

  // Called at the end of BrowserProcessImpl::PreMainMessageLoopRun().
  virtual void PreMainMessageLoopRun();

#if defined(ENABLE_CONFIGURATION_POLICY)
  virtual scoped_ptr<policy::BrowserPolicyConnector>
      CreateBrowserPolicyConnector();
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPartBase);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_BASE_H_
