// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_aurawin.h"

#include "base/command_line.h"
#include "chrome/browser/metro_viewer/metro_viewer_process_host_win.h"
#include "chrome/common/chrome_switches.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart() {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::PlatformSpecificCommandLineProcessing(
    const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kViewerConnection) &&
      !metro_viewer_process_host_.get()) {
    // Tell the metro viewer process host to connect to the given IPC channel.
    metro_viewer_process_host_.reset(
        new MetroViewerProcessHost(
            command_line.GetSwitchValueASCII(switches::kViewerConnection)));
  }
}

void BrowserProcessPlatformPart::StartTearDown() {
}

void BrowserProcessPlatformPart::OnMetroViewerProcessTerminated() {
  metro_viewer_process_host_.reset(NULL);
}
