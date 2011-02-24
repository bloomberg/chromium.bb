// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service/service_process_control.h"

#include "base/command_line.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/service_process_util_posix.h"
#include "third_party/GTM/Foundation/GTMServiceManagement.h"

void ServiceProcessControl::Launcher::DoRun() {
  base::mac::ScopedCFTypeRef<CFDictionaryRef> launchd_plist(
      CreateServiceProcessLaunchdPlist(cmd_line_.get()));
  CFErrorRef error = NULL;
  if (!GTMSMJobSubmit(launchd_plist, &error)) {
    LOG(ERROR) << error;
    CFRelease(error);
  } else {
    launched_ = true;
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableMethod(this, &Launcher::Notify));
}
