// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_browser_process.h"

#include "base/command_line.h"

FirstRunBrowserProcess::FirstRunBrowserProcess(const CommandLine& command_line)
    : BrowserProcessImpl(command_line) {
}

FirstRunBrowserProcess::~FirstRunBrowserProcess() {
}

GoogleURLTracker* FirstRunBrowserProcess::google_url_tracker() {
  return NULL;
}

IntranetRedirectDetector* FirstRunBrowserProcess::intranet_redirect_detector() {
  return NULL;
}
