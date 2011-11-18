// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/setting_change.h"

namespace protector {

SettingChange::SettingChange() {
}

SettingChange::~SettingChange() {
}

bool SettingChange::Init(Protector* protector) {
  return true;
}

void SettingChange::Apply(Protector* protector) {
}

void SettingChange::Discard(Protector* protector) {
}

}  // namespace protector
