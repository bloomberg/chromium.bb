// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/cache_counter.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/signin/identity_test_environment_chrome_browser_state_adaptor.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::Return;

@interface ClearBrowsingDataCollectionViewController (ExposedForTesting)
- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;
@end

namespace {

enum ItemEnum {
  kDeleteBrowsingHistoryItem,
  kDeleteCookiesItem,
  kDeleteCacheItem,
  kDeletePasswordsItem,
  kDeleteFormDataItem
};

class ClearBrowsingDataCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();

    // Setup identity services.
    TestChromeBrowserState::TestingFactories factories = {
        {ProfileSyncServiceFactory::GetInstance(),
         base::BindRepeating(&BuildMockProfileSyncService)},
    };
    browser_state_ = IdentityTestEnvironmentChromeBrowserStateAdaptor::
        CreateChromeBrowserStateForIdentityTestEnvironment(factories);

    identity_test_env_adaptor_.reset(
        new IdentityTestEnvironmentChromeBrowserStateAdaptor(
            browser_state_.get()));

    mock_sync_service_ = static_cast<browser_sync::ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  CollectionViewController* InstantiateController() override {
    return [[ClearBrowsingDataCollectionViewController alloc]
        initWithBrowserState:browser_state_.get()];
  }

  void SelectItem(int item, int section) {
    NSIndexPath* indexPath =
        [NSIndexPath indexPathForItem:item inSection:section];
    [controller() collectionView:[controller() collectionView]
        didSelectItemAtIndexPath:indexPath];
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<IdentityTestEnvironmentChromeBrowserStateAdaptor>
      identity_test_env_adaptor_;
  browser_sync::ProfileSyncServiceMock* mock_sync_service_;
};

// Tests ClearBrowsingDataCollectionViewControllerTest is set up with all
// appropriate items and sections.
TEST_F(ClearBrowsingDataCollectionViewControllerTest, TestModel) {
  EXPECT_CALL(*mock_sync_service_, GetDisableReasons())
      .WillRepeatedly(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  CreateController();
  CheckController();

  int section_offset = 0;
  if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
    section_offset = 1;
  }

  CheckTextCellTitleWithId(IDS_IOS_CLEAR_BROWSING_HISTORY, 0 + section_offset,
                           0);
  CheckAccessoryType(MDCCollectionViewCellAccessoryCheckmark,
                     0 + section_offset, 0);
  CheckTextCellTitleWithId(IDS_IOS_CLEAR_COOKIES, 0 + section_offset, 1);
  CheckAccessoryType(MDCCollectionViewCellAccessoryCheckmark,
                     0 + section_offset, 1);
  CheckTextCellTitleWithId(IDS_IOS_CLEAR_CACHE, 0 + section_offset, 2);
  CheckAccessoryType(MDCCollectionViewCellAccessoryCheckmark,
                     0 + section_offset, 2);
  CheckTextCellTitleWithId(IDS_IOS_CLEAR_SAVED_PASSWORDS, 0 + section_offset,
                           3);
  CheckAccessoryType(MDCCollectionViewCellAccessoryNone, 0 + section_offset, 3);
  CheckTextCellTitleWithId(IDS_IOS_CLEAR_AUTOFILL, 0 + section_offset, 4);
  CheckAccessoryType(MDCCollectionViewCellAccessoryNone, 0 + section_offset, 4);

  CheckTextCellTitleWithId(IDS_IOS_CLEAR_BUTTON, 1 + section_offset, 0);

  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SAVED_SITE_DATA,
                           2 + section_offset);
}

TEST_F(ClearBrowsingDataCollectionViewControllerTest,
       TestItemsSignedInSyncOff) {
  EXPECT_CALL(*mock_sync_service_, GetDisableReasons())
      .WillRepeatedly(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));
  identity_test_env()->SetPrimaryAccount("syncuser@example.com");
  CreateController();
  CheckController();

  int section_offset = 0;
  if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
    EXPECT_EQ(5, NumberOfSections());
    EXPECT_EQ(1, NumberOfItemsInSection(0));
    section_offset = 1;
  } else {
    EXPECT_EQ(4, NumberOfSections());
  }

  EXPECT_EQ(5, NumberOfItemsInSection(0 + section_offset));
  EXPECT_EQ(1, NumberOfItemsInSection(1 + section_offset));

  EXPECT_EQ(1, NumberOfItemsInSection(2 + section_offset));
  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT, 2);

  EXPECT_EQ(1, NumberOfItemsInSection(3 + section_offset));
  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SAVED_SITE_DATA,
                           3 + section_offset);
}

TEST_F(ClearBrowsingDataCollectionViewControllerTest,
       TestItemsSignedInSyncActiveHistoryOff) {
  EXPECT_CALL(*mock_sync_service_, GetDisableReasons())
      .WillRepeatedly(Return(syncer::SyncService::DISABLE_REASON_NONE));
  EXPECT_CALL(*mock_sync_service_, GetTransportState())
      .WillRepeatedly(Return(syncer::SyncService::TransportState::ACTIVE));
  EXPECT_CALL(*mock_sync_service_->GetUserSettingsMock(),
              IsFirstSetupComplete())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sync_service_, GetActiveDataTypes())
      .WillRepeatedly(Return(syncer::ModelTypeSet()));
  EXPECT_CALL(*mock_sync_service_->GetUserSettingsMock(),
              IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));

  identity_test_env()->SetPrimaryAccount("syncuser@example.com");
  CreateController();
  CheckController();

  int section_offset = 0;
  if (experimental_flags::IsNewClearBrowsingDataUIEnabled()) {
    section_offset = 1;
  }

  CheckSectionFooterWithId(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT,
                           2 + section_offset);

  CheckSectionFooterWithId(
      IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_CLEAR_SYNC_AND_SAVED_SITE_DATA,
      3 + section_offset);
}

TEST_F(ClearBrowsingDataCollectionViewControllerTest, TestUpdatePrefWithValue) {
  CreateController();
  CheckController();
  PrefService* prefs = browser_state_->GetPrefs();

  const int section_offset =
      experimental_flags::IsNewClearBrowsingDataUIEnabled() ? 1 : 0;

  SelectItem(kDeleteBrowsingHistoryItem, 0 + section_offset);
  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory));
  SelectItem(kDeleteCookiesItem, 0 + section_offset);
  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteCookies));
  SelectItem(kDeleteCacheItem, 0 + section_offset);
  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteCache));
  SelectItem(kDeletePasswordsItem, 0 + section_offset);
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeletePasswords));
  SelectItem(kDeleteFormDataItem, 0 + section_offset);
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteFormData));
}

}  // namespace
