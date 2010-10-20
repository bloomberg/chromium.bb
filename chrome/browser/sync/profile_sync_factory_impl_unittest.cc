// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"
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
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    FilePath program_path(FILE_PATH_LITERAL("chrome.exe"));
    command_line_.reset(new CommandLine(program_path));
    profile_sync_service_factory_.reset(
        new ProfileSyncFactoryImpl(profile_.get(), command_line_.get()));
  }

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<ProfileSyncFactoryImpl> profile_sync_service_factory_;
};

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDefault) {
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(7U, controller_states_ptr->size());
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PASSWORDS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableAutofill) {
  command_line_->AppendSwitch(switches::kDisableSyncAutofill);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(6U, controller_states_ptr->size());
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(0U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PASSWORDS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableBookmarks) {
  command_line_->AppendSwitch(switches::kDisableSyncBookmarks);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(6U, controller_states_ptr->size());
  EXPECT_EQ(0U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PASSWORDS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisablePreferences) {
  command_line_->AppendSwitch(switches::kDisableSyncPreferences);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(6U, controller_states_ptr->size());
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(0U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PASSWORDS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableThemes) {
  command_line_->AppendSwitch(switches::kDisableSyncThemes);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(6U, controller_states_ptr->size());
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(0U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PASSWORDS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableExtensions) {
  command_line_->AppendSwitch(switches::kDisableSyncExtensions);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(6U, controller_states_ptr->size());
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(0U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PASSWORDS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableApps) {
  command_line_->AppendSwitch(switches::kDisableSyncApps);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(6U, controller_states_ptr->size());
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(0U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PASSWORDS));
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisablePasswords) {
  command_line_->AppendSwitch(switches::kDisableSyncPasswords);
  scoped_ptr<ProfileSyncService> pss;
  pss.reset(profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  DataTypeController::StateMap* controller_states_ptr = &controller_states;
  pss->GetDataTypeControllerStates(controller_states_ptr);
  EXPECT_EQ(6U, controller_states_ptr->size());
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::BOOKMARKS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::PREFERENCES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::AUTOFILL));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::THEMES));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::EXTENSIONS));
  EXPECT_EQ(1U, controller_states_ptr->count(syncable::APPS));
  EXPECT_EQ(0U, controller_states_ptr->count(syncable::PASSWORDS));
}
