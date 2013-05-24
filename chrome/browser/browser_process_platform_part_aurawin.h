// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_AURAWIN_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_AURAWIN_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process_platform_part_base.h"

class ChromeMetroViewerProcessHost;

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPart();
  virtual ~BrowserProcessPlatformPart();

  // Invoked when the ASH metro viewer process on Windows 8 exits.
  void OnMetroViewerProcessTerminated();

  // Overridden from BrowserProcessPlatformPartBase:
  virtual void PlatformSpecificCommandLineProcessing(
      const CommandLine& command_line) OVERRIDE;
  virtual void AttemptExit() OVERRIDE;

 private:
  // Hosts the channel for the Windows 8 metro viewer process which runs in
  // the ASH environment.
  scoped_ptr<ChromeMetroViewerProcessHost> metro_viewer_process_host_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_AURAWIN_H_
