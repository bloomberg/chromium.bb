// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/base_setting_change.h"

namespace protector {

BaseSettingChange::BaseSettingChange() {
}

BaseSettingChange::~BaseSettingChange() {
}

bool BaseSettingChange::Init(Protector* protector) {
  return true;
}

void BaseSettingChange::Apply(Protector* protector) {
}

void BaseSettingChange::Discard(Protector* protector) {
}

}  // namespace protector
