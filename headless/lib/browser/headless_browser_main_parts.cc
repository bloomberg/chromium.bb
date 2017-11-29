// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_net_log.h"
#include "headless/lib/browser/headless_screen.h"

namespace headless {

HeadlessBrowserMainParts::HeadlessBrowserMainParts(HeadlessBrowserImpl* browser)
    : browser_(browser)
    , devtools_http_handler_started_(false) {}

HeadlessBrowserMainParts::~HeadlessBrowserMainParts() = default;

void HeadlessBrowserMainParts::PreMainMessageLoopRun() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLogNetLog)) {
    base::FilePath log_path =
        command_line->GetSwitchValuePath(switches::kLogNetLog);
    net_log_.reset(new HeadlessNetLog(log_path));
  } else {
    net_log_.reset(new net::NetLog());
  }

  if (browser_->options()->DevtoolsServerEnabled()) {
    StartLocalDevToolsHttpHandler(browser_->options());
    devtools_http_handler_started_ = true;
  }
  browser_->PlatformInitialize();
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_http_handler_started_) {
    StopLocalDevToolsHttpHandler();
    devtools_http_handler_started_ = false;
  }
}

}  // namespace headless
