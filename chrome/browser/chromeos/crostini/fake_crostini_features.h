// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_FEATURES_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_FEATURES_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"

class Profile;

namespace crostini {

// FakeCrostiniFeatures implements a fake version of CrostiniFeatures which can
// be used for testing.  It captures the current global CrostiniFeatures object
// and replaces it for the scope of this object.  It overrides only the
// features that you set and uses the previous object for other features.
class FakeCrostiniFeatures : public CrostiniFeatures {
 public:
  FakeCrostiniFeatures();
  ~FakeCrostiniFeatures() override;

  // CrostiniFeatures:
  bool IsAllowed(Profile* profile) override;
  bool IsUIAllowed(Profile* profile, bool check_policy) override;
  bool IsEnabled(Profile* profile) override;
  bool IsExportImportUIAllowed(Profile* profile) override;
  bool IsRootAccessAllowed(Profile* profile) override;
  bool IsContainerUpgradeUIAllowed(Profile*) override;
  bool CanChangeAdbSideloading(Profile* profile) override;

  void SetAll(bool flag);
  void ClearAll();

  void set_allowed(bool allowed) {
    allowed_ = allowed;
  }
  void set_ui_allowed(bool allowed) {
    ui_allowed_ = allowed;
  }
  void set_enabled(bool enabled) {
    enabled_ = enabled;
  }
  void set_export_import_ui_allowed(bool allowed) {
    export_import_ui_allowed_ = allowed;
  }
  void set_root_access_allowed(bool allowed) {
    root_access_allowed_ = allowed;
  }
  void set_container_upgrade_ui_allowed(bool allowed) {
    container_upgrade_ui_allowed_ = allowed;
  }

  void set_can_change_adb_sideloading(bool can_change) {
    can_change_adb_sideloading_ = can_change;
  }

 private:
  // Original global static when this instance is created. It is captured when
  // FakeCrostiniFeatures is created and replaced at destruction.
  CrostiniFeatures* original_features_;

  base::Optional<bool> allowed_;
  base::Optional<bool> ui_allowed_;
  base::Optional<bool> enabled_;
  base::Optional<bool> export_import_ui_allowed_;
  base::Optional<bool> root_access_allowed_;
  base::Optional<bool> container_upgrade_ui_allowed_;
  base::Optional<bool> can_change_adb_sideloading_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_FEATURES_H_
