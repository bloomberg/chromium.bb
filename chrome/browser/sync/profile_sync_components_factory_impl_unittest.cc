// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/profile_sync_components_factory_impl.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "components/sync_driver/sync_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_switches.h"

using sync_driver::DataTypeController;

class ProfileSyncComponentsFactoryImplTest : public testing::Test {
 protected:
  ProfileSyncComponentsFactoryImplTest()
      : thread_bundle_(content::TestBrowserThreadBundle::DEFAULT) {}

  void SetUp() override {
    profile_.reset(new TestingProfile());
    base::FilePath program_path(FILE_PATH_LITERAL("chrome.exe"));
    command_line_.reset(new base::CommandLine(program_path));
    scope_set_.insert(GaiaConstants::kChromeSyncOAuth2Scope);
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
    datatypes.push_back(syncer::AUTOFILL_WALLET_DATA);
    datatypes.push_back(syncer::BOOKMARKS);
    datatypes.push_back(syncer::DEVICE_INFO);
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
    datatypes.push_back(syncer::SUPERVISED_USERS);
    datatypes.push_back(syncer::SUPERVISED_USER_SETTINGS);
    datatypes.push_back(syncer::SUPERVISED_USER_SHARED_SETTINGS);
    datatypes.push_back(syncer::SUPERVISED_USER_WHITELISTS);

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
    GURL sync_service_url =
        GetSyncServiceURL(*command_line_, chrome::GetChannel());
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    scoped_ptr<sync_driver::SyncApiComponentFactory> factory(
        new ProfileSyncComponentsFactoryImpl(
            profile_.get(), command_line_.get(),
            GetSyncServiceURL(*command_line_, chrome::GetChannel()),
            token_service, profile_->GetRequestContext()));
    scoped_ptr<sync_driver::SyncClient> sync_client(
        new browser_sync::ChromeSyncClient(profile_.get(), factory.Pass()));
    scoped_ptr<ProfileSyncService> pss(new ProfileSyncService(
        sync_client.Pass(),
        make_scoped_ptr<SigninManagerWrapper>(NULL), token_service,
        browser_sync::MANUAL_START, base::Bind(&EmptyNetworkTimeUpdate),
        profile_->GetPath(), profile_->GetRequestContext(),
        profile_->GetDebugName(), chrome::GetChannel(),
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::DB),
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::FILE),
        content::BrowserThread::GetBlockingPool()));
    pss->GetSyncClient()->Initialize(pss.get());
    DataTypeController::StateMap controller_states;
    pss->GetDataTypeControllerStates(&controller_states);
    EXPECT_EQ(DefaultDatatypesCount() - types.Size(), controller_states.size());
    CheckDefaultDatatypesInMapExcept(&controller_states, types);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<base::CommandLine> command_line_;
  OAuth2TokenService::ScopeSet scope_set_;
};

TEST_F(ProfileSyncComponentsFactoryImplTest, CreatePSSDefault) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
  scoped_ptr<sync_driver::SyncApiComponentFactory> factory(
      new ProfileSyncComponentsFactoryImpl(
          profile_.get(), command_line_.get(),
          GetSyncServiceURL(*command_line_, chrome::GetChannel()),
          token_service, profile_->GetRequestContext()));
  scoped_ptr<sync_driver::SyncClient> sync_client(
      new browser_sync::ChromeSyncClient(profile_.get(), factory.Pass()));
  scoped_ptr<ProfileSyncService> pss(new ProfileSyncService(
      sync_client.Pass(),
      make_scoped_ptr<SigninManagerWrapper>(NULL), token_service,
      browser_sync::MANUAL_START, base::Bind(&EmptyNetworkTimeUpdate),
      profile_->GetPath(), profile_->GetRequestContext(),
      profile_->GetDebugName(), chrome::GetChannel(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::DB),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE),
      content::BrowserThread::GetBlockingPool()));
  pss->GetSyncClient()->Initialize(pss.get());
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
