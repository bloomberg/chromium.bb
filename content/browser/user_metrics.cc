// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/user_metrics.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

using content::BrowserThread;

void UserMetrics::RecordAction(const UserMetricsAction& action) {
  Record(action.str_);
}

void UserMetrics::RecordComputedAction(const std::string& action) {
  Record(action.c_str());
}

void UserMetrics::Record(const char *action) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&UserMetrics::CallRecordOnUI, action));
    return;
  }

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_USER_ACTION,
      content::NotificationService::AllSources(),
      content::Details<const char*>(&action));
}

void UserMetrics::CallRecordOnUI(const std::string& action) {
  Record(action.c_str());
}
