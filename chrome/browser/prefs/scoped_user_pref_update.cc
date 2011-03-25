// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/scoped_user_pref_update.h"

#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"

ScopedUserPrefUpdate::ScopedUserPrefUpdate(
    PrefService* service, const char* path)
    : service_(service),
      path_(path) {}

ScopedUserPrefUpdate::~ScopedUserPrefUpdate() {
  service_->ReportUserPrefChanged(path_);
}
