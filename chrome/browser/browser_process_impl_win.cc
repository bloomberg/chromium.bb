// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/browser_process_impl.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

#if defined(USE_AURA)
#include "chrome/browser/metro_viewer/metro_viewer_process_host_win.h"
#endif

void BrowserProcessImpl::PlatformSpecificCommandLineProcessing(
    const CommandLine& command_line) {
#if defined(USE_AURA)
  PerformInitForWindowsAura(command_line);
#endif
}

#if defined(USE_AURA)
void BrowserProcessImpl::OnMetroViewerProcessTerminated() {
  metro_viewer_process_host_.reset(NULL);
}

void BrowserProcessImpl::PerformInitForWindowsAura(
    const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kViewerConnection) &&
      !metro_viewer_process_host_.get()) {
    // Tell the metro viewer process host to connect to the given IPC channel.
    metro_viewer_process_host_.reset(
        new MetroViewerProcessHost(
            command_line.GetSwitchValueASCII(switches::kViewerConnection)));
  }
}
#endif
