// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/scoped_pref_update.h"

ScopedPrefUpdate::ScopedPrefUpdate(PrefService* service, const wchar_t* path)
    : service_(service),
      path_(path) {}

ScopedPrefUpdate::~ScopedPrefUpdate() {
  service_->FireObservers(path_.c_str());
}
