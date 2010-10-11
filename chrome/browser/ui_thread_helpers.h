// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THREAD_HELPERS_H_
#define CHROME_BROWSER_UI_THREAD_HELPERS_H_
#pragma once

class Task;

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace ui_thread_helpers {

// This can be used in place of BrowserThread::PostTask(BrowserThread::UI, ...).
// The purpose of this function is to be able to execute Chrome work alongside
// native work when a message loop is running nested or, in the case of Mac,
// in a different mode.  Currently this is used for updating the HostZoomMap
// while the Wrench menu is open, allowing for the zoom display to update.  See
// http://crbug.com/48679 for the full rationale.
//
// CAVEAT EMPTOR: This function's implementation is different across platforms
// and may run the Task in a way that differs from the stock MessageLoop.  You
// should check the behavior on all platforms if you use this.
bool PostTaskWhileRunningMenu(const tracked_objects::Location& from_here,
                              Task* task);

}  // namespace ui_thread_helpers

#endif  // CHROME_BROWSER_UI_THREAD_HELPERS_H_
