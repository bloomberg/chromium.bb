// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync_driver/data_type_controller.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_switches.h"

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
#if defined(ENABLE_APP_LIST)
    if (app_list::switches::IsAppListSyncEnabled())
      datatypes.push_back(syncer::APP_LIST);
#endif
    datatypes.push_back(syncer::APP_SETTINGS);
    datatypes.push_back(syncer::AUTOFILL);
    datatypes.push_back(syncer::AUTOFILL_PROFILE);
    datatypes.push_back(syncer::BOOKMARKS);
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
    datatypes.push_back(syncer::DICTIONARY);
#endif
    datatypes.push_back(syncer::EXTENSIONS);
    datatypes.push_back(syncer::EXTENSION_SETTINGS);
    datatypes.push_back(syncer::HISTORY_DELETE_DIRECTIVES);
    datatypes.push_back(syncer::PASSWORDS);
    datatypes.push_back(syncer::PREFERENCES);
    datatypes.push_back(syncer::PRIORITY_PREFERENCES);
    datatypes.push_back(syncer::SEARCH_ENGINES);
    datatypes.push_back(syncer::SESSIONS);
    datatypes.push_back(syncer::PROXY_TABS);
    datatypes.push_back(syncer::THEMES);
    datatypes.push_back(syncer::TYPED_URLS);
    datatypes.push_back(syncer::FAVICON_TRACKING);
    datatypes.push_back(syncer::FAVICON_IMAGES);
    datatypes.push_back(syncer::SYNCED_NOTIFICATIONS);
    // TODO(petewil): Enable on stable once we have tested on stable.
    chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
    if (channel == chrome::VersionInfo::CHANNEL_UNKNOWN ||
        channel == chrome::VersionInfo::CHANNEL_DEV ||
        channel == chrome::VersionInfo::CHANNEL_CANARY) {
      datatypes.push_back(syncer::SYNCED_NOTIFICATION_APP_INFO);
    }
    datatypes.push_back(syncer::MANAGED_USERS);
    datatypes.push_back(syncer::MANAGED_USER_SHARED_SETTINGS);

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
      syncer::ModelTypeSet exception_types) {
    std::vector<syncer::ModelType> defaults = DefaultDatatypes();
    std::vector<syncer::ModelType>::iterator iter;
    for (iter = defaults.begin(); iter != defaults.end(); ++iter) {
      if (exception_types.Has(*iter))
        EXPECT_EQ(0U, map->count(*iter))
            << *iter << " found in dataypes map, shouldn't be there.";
      else
        EXPECT_EQ(1U, map->count(*iter))
            << *iter << " not found in datatypes map";
    }
  }

  // Asserts that if you disable types via the command line, all other types
  // are enabled.
  void TestSwitchDisablesType(syncer::ModelTypeSet types) {
    command_line_->AppendSwitchASCII(switches::kDisableSyncTypes,
                                     syncer::ModelTypeSetToString(types));
    scoped_ptr<ProfileSyncService> pss(
        new ProfileSyncService(
            new ProfileSyncComponentsFactoryImpl(profile_.get(),
                                                 command_line_.get()),
            profile_.get(),
            NULL,
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get()),
            browser_sync::MANUAL_START));
    pss->factory()->RegisterDataTypes(pss.get());
    DataTypeController::StateMap controller_states;
    pss->GetDataTypeControllerStates(&controller_states);
    EXPECT_EQ(DefaultDatatypesCount() - types.Size(), controller_states.size());
    CheckDefaultDatatypesInMapExcept(&controller_states, types);
  }

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<CommandLine> command_line_;
};

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDefault) {
  scoped_ptr<ProfileSyncService> pss(new ProfileSyncService(
      new ProfileSyncComponentsFactoryImpl(profile_.get(), command_line_.get()),
      profile_.get(),
      NULL,
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get()),
      browser_sync::MANUAL_START));
  pss->factory()->RegisterDataTypes(pss.get());
  DataTypeController::StateMap controller_states;
  pss->GetDataTypeControllerStates(&controller_states);
  EXPECT_EQ(DefaultDatatypesCount(), controller_states.size());
  CheckDefaultDatatypesInMapExcept(&controller_states, syncer::ModelTypeSet());
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableOne) {
  TestSwitchDisablesType(syncer::ModelTypeSet(syncer::AUTOFILL));
}

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDisableMultiple) {
  TestSwitchDisablesType(
      syncer::ModelTypeSet(syncer::AUTOFILL_PROFILE, syncer::BOOKMARKS));
}
