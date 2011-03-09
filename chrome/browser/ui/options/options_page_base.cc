// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/options/options_page_base.h"

#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsPageBase

OptionsPageBase::OptionsPageBase(Profile* profile)
    : profile_(profile) {
}

OptionsPageBase::~OptionsPageBase() {
}

void OptionsPageBase::UserMetricsRecordAction(const UserMetricsAction& action,
                                              PrefService* prefs) {
  UserMetrics::RecordAction(action, profile());
  if (prefs)
    prefs->ScheduleSavePersistentPrefs();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsPageBase, NotificationObserver implementation:

void OptionsPageBase::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::string>(details).ptr());
}
