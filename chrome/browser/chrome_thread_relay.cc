// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_thread_relay.h"

ChromeThreadRelay::ChromeThreadRelay() {
  if (!ChromeThread::GetCurrentThreadIdentifier(&origin_thread_id_))
    NOTREACHED() << "Must be created on a valid ChromeThread";
}

void ChromeThreadRelay::Start(ChromeThread::ID target_thread_id,
                              const tracked_objects::Location& from_here) {
  ChromeThread::PostTask(
      target_thread_id,
      from_here,
      NewRunnableMethod(this, &ChromeThreadRelay::ProcessOnTargetThread));
}

void ChromeThreadRelay::ProcessOnTargetThread() {
  RunWork();
  ChromeThread::PostTask(
      origin_thread_id_,
      FROM_HERE,
      NewRunnableMethod(this, &ChromeThreadRelay::RunCallback));
}
