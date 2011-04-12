// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/scoped_user_pref_update.h"

#include "base/logging.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"

namespace subtle {

ScopedUserPrefUpdateBase::ScopedUserPrefUpdateBase(PrefService* service,
                                                   const char* path)
    : service_(service),
      path_(path),
      value_(NULL) {}

ScopedUserPrefUpdateBase::~ScopedUserPrefUpdateBase() {
  Notify();
}

Value* ScopedUserPrefUpdateBase::Get(Value::ValueType type) {
  if (!value_)
    value_ = service_->GetMutableUserPref(path_.c_str(), type);
  return value_;
}

void ScopedUserPrefUpdateBase::Notify() {
  if (value_) {
    service_->ReportUserPrefChanged(path_);
    value_ = NULL;
  }
}

}  // namespace subtle
