// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"

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

  // Returns the collection of default datatypes.
  static std::vector<syncable::ModelType> DefaultDatatypes() {
    std::vector<syncable::ModelType> datatypes;
    datatypes.push_back(syncable::BOOKMARKS);
    datatypes.push_back(syncable::PREFERENCES);
    datatypes.push_back(syncable::AUTOFILL);
    datatypes.push_back(syncable::THEMES);
    datatypes.push_back(syncable::EXTENSIONS);
    datatypes.push_back(syncable::APPS);
    datatypes.push_back(syncable::AUTOFILL_PROFILE);
    datatypes.push_back(syncable::PASSWORDS);
    return datatypes;
  }

  // Returns the number of default datatypes.
  static size_t DefaultDatatypesCount() {
    return DefaultDatatypes().size();
  }

  // Asserts that all the default datatypes are in |map|, except
  // for |exception_type|, which unless it is UNDEFINED, is asserted to
  // not be in |map|.
  static void CheckDefaultDatatypesInMapExcept(
      DataTypeController::StateMap* map,
      syncable::ModelType exception_type) {
    std::vector<syncable::ModelType> defaults = DefaultDatatypes();
    std::vector<syncable::ModelType>::iterator iter;
    for (iter = defaults.begin(); iter != defaults.end(); ++iter) {
      if (exception_type != syncable::UNSPECIFIED && exception_type == *iter)
        EXPECT_EQ(0U, map->count(*iter))
            << *iter << " found in dataypes map, shouldn't be there.";
      else
        EXPECT_EQ(1U, map->count(*iter))
            << *iter << " not found in datatypes map";
    }
  }

  // Asserts that if you apply the command line switch |cmd_switch|,
  // all types are enabled except for |type|, which is disabled.
  void TestSwitchDisablesType(const char* cmd_switch,
                              syncable::ModelType type) {
    command_line_->AppendSwitch(cmd_switch);
    scoped_ptr<ProfileSyncService> pss(
        profile_sync_service_factory_->CreateProfileSyncService(""));
    DataTypeController::StateMap controller_states;
    pss->GetDataTypeControllerStates(&controller_states);
    EXPECT_EQ(DefaultDatatypesCount() - 1, controller_states.size());
    CheckDefaultDatatypesInMapExcept(&controller_states, type);
  }

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<ProfileSyncFactoryImpl> profile_sync_service_factory_;
};

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDefault) {
  scoped_ptr<ProfileSyncService> pss(
      profile_sync_service_factory_->CreateProfileSyncService(""));
  DataTypeController::StateMap controller_states;
  pss->GetDataTypeControllerStates(&controller_states);
  EXPECT_EQ(DefaultDatatypesCount(), controller_states.size());
  CheckDefaultDatatypesInMapExcept(&controller_states, syncable::UNSPECIFIED);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableAutofill) {
  TestSwitchDisablesType(switches::kDisableSyncAutofill,
                         syncable::AUTOFILL);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableBookmarks) {
  TestSwitchDisablesType(switches::kDisableSyncBookmarks,
                         syncable::BOOKMARKS);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisablePreferences) {
  TestSwitchDisablesType(switches::kDisableSyncPreferences,
                         syncable::PREFERENCES);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableThemes) {
  TestSwitchDisablesType(switches::kDisableSyncThemes,
                         syncable::THEMES);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableExtensions) {
  TestSwitchDisablesType(switches::kDisableSyncExtensions,
                         syncable::EXTENSIONS);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableApps) {
  TestSwitchDisablesType(switches::kDisableSyncApps,
                         syncable::APPS);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisableAutofillProfile) {
  TestSwitchDisablesType(switches::kDisableSyncAutofillProfile,
                         syncable::AUTOFILL_PROFILE);
}

TEST_F(ProfileSyncFactoryImplTest, CreatePSSDisablePasswords) {
  TestSwitchDisablesType(switches::kDisableSyncPasswords,
                         syncable::PASSWORDS);
}
