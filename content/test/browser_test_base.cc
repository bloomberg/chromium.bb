// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/browser_test_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "sandbox/win/src/dep.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/system_monitor/system_monitor.h"
#endif

extern int BrowserMain(const content::MainFunctionParams&);

BrowserTestBase::BrowserTestBase() {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAmIBundled(true);
  base::SystemMonitor::AllocateSystemIOPorts();
#endif
}

BrowserTestBase::~BrowserTestBase() {
}

void BrowserTestBase::SetUp() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

  content::MainFunctionParams params(*command_line);
  params.ui_task =
      new base::Closure(
          base::Bind(&BrowserTestBase::ProxyRunTestOnMainThreadLoop, this));

  SetUpInProcessBrowserTestFixture();
  BrowserMain(params);
  TearDownInProcessBrowserTestFixture();
}

void BrowserTestBase::TearDown() {
}

void BrowserTestBase::ProxyRunTestOnMainThreadLoop() {
  RunTestOnMainThreadLoop();
}
