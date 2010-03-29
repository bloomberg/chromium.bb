// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_profile.h"

using browser_sync::DataTypeController;

class ProfileSyncFactoryImplTest : public testing::Test {
 protected:
  ProfileSyncFactoryImplTest()
      : ui_thread_(ChromeThread::UI, &message_loop_) {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    FilePath program_path(FILE_PATH_LITERAL("chrome.exe"));
    command_line_.reset(new CommandLine(program_path));
    profile_sync_service_factory_.reset(
        new ProfileSyncFactoryImpl(profile_.get(), command_line_.get()));
  }

  MessageLoop message_loop_;
  ChromeThread ui_thread_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<ProfileSyncFactoryImpl> profile_sync_service_factory_;
};

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDefault) {
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService());
  DataTypeController::TypeMap controllers(pss->data_type_controllers());
  EXPECT_EQ(1U, controllers.size());
  EXPECT_EQ(1U, controllers.count(syncable::BOOKMARKS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSEnableAutofill) {
  command_line_->AppendSwitch(switches::kEnableSyncAutofill);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService());
  DataTypeController::TypeMap controllers(pss->data_type_controllers());
  EXPECT_EQ(2U, controllers.size());
  EXPECT_EQ(1U, controllers.count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controllers.count(syncable::AUTOFILL));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableBookmarks) {
  command_line_->AppendSwitch(switches::kDisableSyncBookmarks);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService());
  DataTypeController::TypeMap controllers(pss->data_type_controllers());
  EXPECT_EQ(0U, controllers.size());
  EXPECT_EQ(0U, controllers.count(syncable::BOOKMARKS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSEnablePreferences) {
  command_line_->AppendSwitch(switches::kEnableSyncPreferences);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService());
  DataTypeController::TypeMap controllers(pss->data_type_controllers());
  EXPECT_EQ(2U, controllers.size());
  EXPECT_EQ(1U, controllers.count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controllers.count(syncable::PREFERENCES));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSEnableThemes) {
  command_line_->AppendSwitch(switches::kEnableSyncThemes);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService());
  DataTypeController::TypeMap controllers(pss->data_type_controllers());
  EXPECT_EQ(2U, controllers.size());
  EXPECT_EQ(1U, controllers.count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controllers.count(syncable::THEMES));
}
