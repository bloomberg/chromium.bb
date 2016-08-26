// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/component_updater/updater_state_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class UpdaterStateTest : public testing::Test {
 public:
  UpdaterStateTest() {}
  ~UpdaterStateTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdaterStateTest);
};

TEST_F(UpdaterStateTest, MakeInstallerAttributes) {
  // is_machine argument does not make a difference in this test as, the
  // values of the |updater_state| are fake.
  auto updater_state = UpdaterState::Create(false);

  // Sanity check all members.
  updater_state->google_update_version_ = base::Version("1.0");
  updater_state->last_autoupdate_started_ = base::Time::NowFromSystemTime();
  updater_state->last_checked_ = base::Time::NowFromSystemTime();
  updater_state->is_joined_to_domain_ = false;
  updater_state->is_autoupdate_check_enabled_ = true;
  updater_state->chrome_update_policy_ = 1;

  auto installer_attributes = updater_state->MakeInstallerAttributes();

  EXPECT_STREQ("1.0", installer_attributes.at("googleupdatever").c_str());
  EXPECT_STREQ("0", installer_attributes.at("laststarted").c_str());
  EXPECT_STREQ("0", installer_attributes.at("lastchecked").c_str());
  EXPECT_STREQ("0", installer_attributes.at("domainjoined").c_str());
  EXPECT_STREQ("1", installer_attributes.at("autoupdatecheckenabled").c_str());
  EXPECT_STREQ("1", installer_attributes.at("chromeupdatepolicy").c_str());

  // Tests some of the remaining values.
  updater_state = UpdaterState::Create(false);

  // Don't serialize an invalid version if it could not be read.
  updater_state->google_update_version_ = base::Version();
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_EQ(0u, installer_attributes.count("googleupdatever"));

  updater_state->google_update_version_ = base::Version("0.0.0.0");
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("0.0.0.0", installer_attributes.at("googleupdatever").c_str());

  updater_state->last_autoupdate_started_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(15);
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("408", installer_attributes.at("laststarted").c_str());

  updater_state->last_autoupdate_started_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(90);
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("1344", installer_attributes.at("laststarted").c_str());

  // Don't serialize the time if it could not be read.
  updater_state->last_autoupdate_started_ = base::Time();
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_EQ(0u, installer_attributes.count("laststarted"));

  updater_state->last_checked_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(15);
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("408", installer_attributes.at("lastchecked").c_str());

  updater_state->last_checked_ =
      base::Time::NowFromSystemTime() - base::TimeDelta::FromDays(90);
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("1344", installer_attributes.at("lastchecked").c_str());

  // Don't serialize the time if it could not be read.
  updater_state->last_checked_ = base::Time();
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_EQ(0u, installer_attributes.count("lastchecked"));

  updater_state->is_joined_to_domain_ = true;
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("1", installer_attributes.at("domainjoined").c_str());

  updater_state->is_autoupdate_check_enabled_ = false;
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("0", installer_attributes.at("autoupdatecheckenabled").c_str());

  updater_state->chrome_update_policy_ = 0;
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("0", installer_attributes.at("chromeupdatepolicy").c_str());

  updater_state->chrome_update_policy_ = -1;
  installer_attributes = updater_state->MakeInstallerAttributes();
  EXPECT_STREQ("-1", installer_attributes.at("chromeupdatepolicy").c_str());
}

}  // namespace component_updater
