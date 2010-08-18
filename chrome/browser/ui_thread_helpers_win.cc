// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui_thread_helpers.h"

#include "base/task.h"
#include "chrome/browser/chrome_thread.h"

namespace ui_thread_helpers {

bool PostTaskWhileRunningMenu(const tracked_objects::Location& from_here,
                              Task* task) {
  return ChromeThread::PostTask(ChromeThread::UI, from_here, task);
}

}  // namespace ui_thread_helpers
