// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TASK_HELPERS_H_
#define CHROME_BROWSER_COCOA_TASK_HELPERS_H_
#pragma once

class Task;

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace cocoa_utils {

// This can be used in place of BrowserThread::PostTask(BrowserThread::UI, ...).
// The purpose of this function is to be able to execute Task work alongside
// native work when a MessageLoop is blocked by a nested run loop. This function
// will run the Task in both NSEventTrackingRunLoopMode and NSDefaultRunLoopMode
// for the purpose of executing work while a menu is open. See
// http://crbug.com/48679 for the full rationale.
bool PostTaskInEventTrackingRunLoopMode(
    const tracked_objects::Location& from_here,
    Task* task);

}  // namespace cocoa_utils

#endif  // CHROME_BROWSER_COCOA_TASK_HELPERS_H_
