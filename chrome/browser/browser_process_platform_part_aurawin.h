// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_AURAWIN_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_AURAWIN_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class CommandLine;
class MetroViewerProcessHost;

class BrowserProcessPlatformPart {
 public:
  BrowserProcessPlatformPart();
  virtual ~BrowserProcessPlatformPart();

  // Called after creating the process singleton or when another chrome
  // rendez-vous with this one.
  virtual void PlatformSpecificCommandLineProcessing(
      const CommandLine& command_line);

  // Called from BrowserProcessImpl::StartTearDown().
  virtual void StartTearDown();

  void OnMetroViewerProcessTerminated();

 private:
  // Hosts the channel for the Windows 8 metro viewer process which runs in
  // the ASH environment.
  scoped_ptr<MetroViewerProcessHost> metro_viewer_process_host_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_AURAWIN_H_
