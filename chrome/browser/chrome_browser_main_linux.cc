// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_linux.h"

#include <fontconfig/fontconfig.h>

#include "chrome/browser/browser_process.h"
#include "components/crash/app/breakpad_linux.h"
#include "components/metrics/metrics_service.h"

#if !defined(OS_CHROMEOS)
#include "base/linux_util.h"
#include "chrome/browser/sxs_linux.h"
#include "content/public/browser/browser_thread.h"
#endif

ChromeBrowserMainPartsLinux::ChromeBrowserMainPartsLinux(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsPosix(parameters) {
}

ChromeBrowserMainPartsLinux::~ChromeBrowserMainPartsLinux() {
}

void ChromeBrowserMainPartsLinux::ToolkitInitialized() {
  // Explicitly initialize Fontconfig early on to prevent races later due to
  // implicit initialization in response to threads' first calls to Fontconfig:
  // http://crbug.com/404311
  FcInit();

  ChromeBrowserMainPartsPosix::ToolkitInitialized();
}

void ChromeBrowserMainPartsLinux::PreProfileInit() {
#if !defined(OS_CHROMEOS)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.  This happens in PreCreateThreads.
  // base::GetLinuxDistro() will initialize its value if needed.
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::GetLinuxDistro)));

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&sxs_linux::AddChannelMarkToUserDataDir));
#endif

  ChromeBrowserMainPartsPosix::PreProfileInit();
}

void ChromeBrowserMainPartsLinux::PostProfileInit() {
  ChromeBrowserMainPartsPosix::PostProfileInit();

  g_browser_process->metrics_service()->RecordBreakpadRegistration(
      breakpad::IsCrashReporterEnabled());
}
