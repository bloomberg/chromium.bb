// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/base_setting_change.h"

#include "base/logging.h"

namespace protector {

BaseSettingChange::BaseSettingChange()
    : protector_(NULL) {
}

BaseSettingChange::~BaseSettingChange() {
}

bool BaseSettingChange::Init(Protector* protector) {
  DCHECK(protector);
  protector_ = protector;
  return true;
}

void BaseSettingChange::Apply() {
}

void BaseSettingChange::Discard() {
}

void BaseSettingChange::OnBeforeRemoved() {
}

}  // namespace protector
