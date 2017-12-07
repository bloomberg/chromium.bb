// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_backup_settings_instance.h"

namespace arc {

FakeBackupSettingsInstance::FakeBackupSettingsInstance() = default;

FakeBackupSettingsInstance::~FakeBackupSettingsInstance() = default;

void FakeBackupSettingsInstance::ClearCallHistory() {
  set_backup_enabled_called_ = false;
}

void FakeBackupSettingsInstance::SetBackupEnabled(bool enabled, bool managed) {
  set_backup_enabled_called_ = true;
  enabled_ = enabled;
  managed_ = managed;
}

}  // namespace arc
