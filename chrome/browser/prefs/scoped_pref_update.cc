// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"  // TODO(viettrungluu): remove
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_pref_update.h"

ScopedPrefUpdate::ScopedPrefUpdate(PrefService* service, const char* path)
    : service_(service),
      path_(path) {}

// TODO(viettrungluu): deprecate and remove #include
ScopedPrefUpdate::ScopedPrefUpdate(PrefService* service, const wchar_t* path)
    : service_(service),
      path_(WideToUTF8(path)) {}

ScopedPrefUpdate::~ScopedPrefUpdate() {
  service_->pref_notifier()->FireObservers(path_.c_str());
}
