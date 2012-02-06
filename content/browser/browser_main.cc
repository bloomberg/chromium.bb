// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main.h"

#include "base/debug/trace_event.h"
#include "content/public/browser/browser_main_runner.h"

// Main routine for running as the Browser process.
int BrowserMain(const content::MainFunctionParams& parameters) {
  TRACE_EVENT_BEGIN_ETW("BrowserMain", 0, "");

  scoped_ptr<content::BrowserMainRunner> main_runner_(
      content::BrowserMainRunner::Create());

  int exit_code = main_runner_->Initialize(parameters);
  if (exit_code >= 0)
    return exit_code;

  exit_code = main_runner_->Run();

  main_runner_->Shutdown();

  TRACE_EVENT_END_ETW("BrowserMain", 0, 0);

  return exit_code;
}
