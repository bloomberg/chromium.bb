// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_CRW_RECURRING_TASK_DELEGATE_H_
#define IOS_WEB_WEB_STATE_CRW_RECURRING_TASK_DELEGATE_H_

// A delegate object to run some recurring task.
@protocol CRWRecurringTaskDelegate

// Runs a recurring task.
- (void)runRecurringTask;

@end

#endif  // IOS_WEB_WEB_STATE_CRW_RECURRING_TASK_DELEGATE_H_
