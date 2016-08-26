// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_UPDATER_STATE_WIN_H_
#define COMPONENTS_COMPONENT_UPDATER_UPDATER_STATE_WIN_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/update_client/update_client.h"

namespace component_updater {

// TODO(sorin): implement this in terms of
// chrome/installer/util/google_update_settings (crbug.com/615187).
class UpdaterState {
 public:
  static std::unique_ptr<UpdaterState> Create(bool is_machine);

  update_client::InstallerAttributes MakeInstallerAttributes() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdaterStateTest, MakeInstallerAttributes);

  explicit UpdaterState(bool is_machine);

  // This function is best-effort. It updates the class members with
  // the relevant values that could be retrieved.
  void ReadState();

  static base::Version GetGoogleUpdateVersion(bool is_machine);
  static bool IsAutoupdateCheckEnabled();
  static bool IsJoinedToDomain();
  static base::Time GetGoogleUpdateLastStartedAU(bool is_machine);
  static base::Time GetGoogleUpdateLastChecked(bool is_machine);
  static base::Time GetGoogleUpdateTimeValue(bool is_machine,
                                             const wchar_t* value_name);
  static int GetChromeUpdatePolicy();

  static std::string NormalizeTimeDelta(const base::TimeDelta& delta);

  bool is_machine_;
  base::Version google_update_version_;
  base::Time last_autoupdate_started_;
  base::Time last_checked_;
  bool is_joined_to_domain_ = false;
  bool is_autoupdate_check_enabled_ = false;
  int chrome_update_policy_ = 0;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_UPDATER_STATE_WIN_H_
