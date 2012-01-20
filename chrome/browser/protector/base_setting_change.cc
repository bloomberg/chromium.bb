// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/base_setting_change.h"

#include "base/logging.h"

namespace protector {

BaseSettingChange::BaseSettingChange()
    : profile_(NULL) {
}

BaseSettingChange::~BaseSettingChange() {
}

bool BaseSettingChange::Init(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  return true;
}

void BaseSettingChange::Apply() {
}

void BaseSettingChange::Discard() {
}

void BaseSettingChange::Timeout() {
}

}  // namespace protector
