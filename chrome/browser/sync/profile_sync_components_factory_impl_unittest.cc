// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::DataTypeController;
using content::BrowserThread;

class ProfileSyncComponentsFactoryImplTest : public testing::Test {
 protected:
  ProfileSyncComponentsFactoryImplTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    base::FilePath program_path(FILE_PATH_LITERAL("chrome.exe"));
    command_line_.reset(new CommandLine(program_path));
  }

  // Returns the collection of default datatypes.
  static std::vector<syncer::ModelType> DefaultDatatypes() {
    std::vector<syncer::ModelType> datatypes;
    datatypes.push_back(syncer::APPS);
    datatypes.push_back(syncer::APP_NOTIFICATIONS);
    datatypes.push_back(syncer::APP_SETTINGS);
    datatypes.push_back(syncer::AUTOFILL);
    datatypes.push_back(syncer::AUTOFILL_PROFILE);
    datatypes.push_back(syncer::BOOKMARKS);
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
    datatypes.push_back(syncer::DICTIONARY);
#endif
    datatypes.push_back(syncer::EXTENSIONS);
    datatypes.push_back(syncer::EXTENSION_SETTINGS);
    datatypes.push_back(syncer::PASSWORDS);
    datatypes.push_back(syncer::PREFERENCES);
    datatypes.push_back(syncer::SEARCH_ENGINES);
    datatypes.push_back(syncer::SESSIONS);
    datatypes.push_back(syncer::THEMES);
    datatypes.push_back(syncer::TYPED_URLS);
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
      syncer::ModelType exception_type) {
    std::vector<syncer::ModelType> defaults = DefaultDatatypes();
    std::vector<syncer::ModelType>::iterator iter;
    for (iter = defaults.begin(); iter != defaults.end(); ++iter) {
      if (exception_type != syncer::UNSPECIFIED && exception_type == *iter)
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
                              syncer::ModelType type) {
    command_line_->AppendSwitch(cmd_switch);
    scoped_ptr<ProfileSyncService> pss(
        new ProfileSyncService(
            new ProfileSyncComponentsFactoryImpl(profile_.get(),
                                                 command_line_.get()),
            profile_.get(),
            NULL,
            ProfileSyncService::MANUAL_START));
    pss->factory()->RegisterDataTypes(pss.get());
    DataTypeController::StateMap controller_states;
    pss->GetDataTypeControllerStates(&controller_states);
    EXPECT_EQ(DefaultDatatypesCount() - 1, controller_states.size());
    CheckDefaultDatatypesInMapExcept(&controller_states, type);
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<CommandLine> command_line_;
};

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDefault) {
  scoped_ptr<ProfileSyncService> pss(
      new ProfileSyncService(
          new ProfileSyncComponentsFactoryImpl(profile_.get(),
                                               command_line_.get()),
      profile_.get(),
      NULL,
      ProfileSyncService::MANUAL_START));
  pss->factory()->RegisterDataTypes(pss.get());
  DataTypeController::StateMap controller_states;
  pss->GetDataTypeControllerStates(&controller_states);
  EXPECT_EQ(DefaultDatatypesCount(), controller_states.size());
  CheckDefaultDatatypesInMapExcept(&controller_states, syncer::UNSPECIFIED);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableAutofill) {
  TestSwitchDisablesType(switches::kDisableSyncAutofill,
                         syncer::AUTOFILL);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableBookmarks) {
  TestSwitchDisablesType(switches::kDisableSyncBookmarks,
                         syncer::BOOKMARKS);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisablePreferences) {
  TestSwitchDisablesType(switches::kDisableSyncPreferences,
                         syncer::PREFERENCES);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableThemes) {
  TestSwitchDisablesType(switches::kDisableSyncThemes,
                         syncer::THEMES);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableExtensions) {
  TestSwitchDisablesType(switches::kDisableSyncExtensions,
                         syncer::EXTENSIONS);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableApps) {
  TestSwitchDisablesType(switches::kDisableSyncApps,
                         syncer::APPS);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableAutofillProfile) {
  TestSwitchDisablesType(switches::kDisableSyncAutofillProfile,
                         syncer::AUTOFILL_PROFILE);
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisablePasswords) {
  TestSwitchDisablesType(switches::kDisableSyncPasswords,
                         syncer::PASSWORDS);
}
