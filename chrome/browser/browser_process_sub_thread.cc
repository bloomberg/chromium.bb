// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/browser_process_sub_thread.h"
#include "content/common/notification_service.h"

#if defined(OS_WIN)
#include <Objbase.h>
#endif

BrowserProcessSubThread::BrowserProcessSubThread(BrowserThread::ID identifier)
      : BrowserThread(identifier) {}

BrowserProcessSubThread::~BrowserProcessSubThread() {
  // We cannot rely on our base class to stop the thread since we want our
  // CleanUp function to run.
  Stop();
}

void BrowserProcessSubThread::Init() {
#if defined(OS_WIN)
  // Initializes the COM library on the current thread.
  CoInitialize(NULL);
#endif

  notification_service_ = new NotificationService;
}

void BrowserProcessSubThread::CleanUp() {
  delete notification_service_;
  notification_service_ = NULL;

#if defined(OS_WIN)
  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
#endif
}
