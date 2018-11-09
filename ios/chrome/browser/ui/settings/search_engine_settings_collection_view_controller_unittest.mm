// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_settings_collection_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class SearchEngineSettingsCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
    template_url_service_ = ios::TemplateURLServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
    template_url_service_->Load();
  }

  CollectionViewController* InstantiateController() override {
    return [[SearchEngineSettingsCollectionViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  // Adds a prepopulated search engine to TemplateURLService.
  TemplateURL* AddPriorSearchEngine(const std::string& short_name,
                                    int prepopulate_id,
                                    bool set_default) {
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(short_name));
    data.SetURL("https://chromium.test/index.php?q={searchTerms}");
    data.prepopulate_id = prepopulate_id;
    TemplateURL* url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    if (set_default)
      template_url_service_->SetUserSelectedDefaultSearchProvider(url);
    return url;
  }

  // Adds a custom search engine to TemplateURLService.
  TemplateURL* AddCustomSearchEngine(const std::string& short_name,
                                     base::Time last_visited_time,
                                     bool set_default) {
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(short_name));
    data.SetURL("https://chromium.test/index.php?q={searchTerms}");
    data.last_visited = last_visited_time;
    TemplateURL* url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    if (set_default)
      template_url_service_->SetUserSelectedDefaultSearchProvider(url);
    return url;
  }

  // Checks if a text cell in the CollectionView has a check mark.
  void CheckTextCellChecked(bool expect_checked, int section, int item) {
    SettingsTextItem* text_item = base::mac::ObjCCastStrict<SettingsTextItem>(
        GetCollectionViewItem(section, item));
    ASSERT_TRUE(text_item);
    EXPECT_EQ(expect_checked ? MDCCollectionViewCellAccessoryCheckmark
                             : MDCCollectionViewCellAccessoryNone,
              text_item.accessoryType);
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  TemplateURLService* template_url_service_;  // weak
};

// Tests that no items are shown if TemplateURLService is empty.
TEST_F(SearchEngineSettingsCollectionViewControllerTest, TestNoUrl) {
  CreateController();
  CheckController();
  EXPECT_EQ(0, NumberOfSections());
}

// Tests that items are displayed correctly when TemplateURLService is filled
// and a prepopulated search engine is selected as default.
TEST_F(SearchEngineSettingsCollectionViewControllerTest,
       TestUrlsLoadedWithPrepopulatedSearchEngineAsDefault) {
  AddPriorSearchEngine("prepopulated-3", 3, false);
  AddPriorSearchEngine("prepopulated-1", 1, false);
  AddPriorSearchEngine("prepopulated-2", 2, true);

  AddCustomSearchEngine(
      "custom-4", base::Time::Now() - base::TimeDelta::FromDays(10), false);
  AddCustomSearchEngine(
      "custom-1", base::Time::Now() - base::TimeDelta::FromSeconds(10), false);
  AddCustomSearchEngine(
      "custom-3", base::Time::Now() - base::TimeDelta::FromHours(10), false);
  AddCustomSearchEngine(
      "custom-2", base::Time::Now() - base::TimeDelta::FromMinutes(10), false);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(3, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(false, 0, 0);
  CheckTextCellTitle(@"prepopulated-2", 0, 1);
  CheckTextCellChecked(true, 0, 1);
  CheckTextCellTitle(@"prepopulated-3", 0, 2);
  CheckTextCellChecked(false, 0, 2);

  ASSERT_EQ(3, NumberOfItemsInSection(1));
  CheckTextCellTitle(@"custom-1", 1, 0);
  CheckTextCellChecked(false, 1, 0);
  CheckTextCellTitle(@"custom-2", 1, 1);
  CheckTextCellChecked(false, 1, 1);
  CheckTextCellTitle(@"custom-3", 1, 2);
  CheckTextCellChecked(false, 1, 2);
}

// Tests that items are displayed correctly when TemplateURLService is filled
// and a custom search engine is selected as default.
TEST_F(SearchEngineSettingsCollectionViewControllerTest,
       TestUrlsLoadedWithCustomSearchEngineAsDefault) {
  AddPriorSearchEngine("prepopulated-3", 3, false);
  AddPriorSearchEngine("prepopulated-1", 1, false);
  AddPriorSearchEngine("prepopulated-2", 2, false);

  AddCustomSearchEngine(
      "custom-4", base::Time::Now() - base::TimeDelta::FromDays(10), false);
  AddCustomSearchEngine(
      "custom-1", base::Time::Now() - base::TimeDelta::FromSeconds(10), false);
  AddCustomSearchEngine(
      "custom-3", base::Time::Now() - base::TimeDelta::FromHours(10), false);
  AddCustomSearchEngine(
      "custom-2", base::Time::Now() - base::TimeDelta::FromMinutes(10), true);

  CreateController();
  CheckController();

  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(4, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(false, 0, 0);
  CheckTextCellTitle(@"prepopulated-2", 0, 1);
  CheckTextCellChecked(false, 0, 1);
  CheckTextCellTitle(@"prepopulated-3", 0, 2);
  CheckTextCellChecked(false, 0, 2);
  CheckTextCellTitle(@"custom-2", 0, 3);
  CheckTextCellChecked(true, 0, 3);

  ASSERT_EQ(2, NumberOfItemsInSection(1));
  CheckTextCellTitle(@"custom-1", 1, 0);
  CheckTextCellChecked(false, 1, 0);
  CheckTextCellTitle(@"custom-3", 1, 1);
  CheckTextCellChecked(false, 1, 1);
}

// Tests that when TemplateURLService add or remove TemplateURLs, or update
// default search engine, the controller will update the displayed items.
TEST_F(SearchEngineSettingsCollectionViewControllerTest,
       TestUrlModifiedByService) {
  TemplateURL* url_1 = AddPriorSearchEngine("prepopulated-1", 1, true);

  CreateController();
  CheckController();

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(true, 0, 0);

  TemplateURL* url_2 = AddPriorSearchEngine("prepopulated-2", 2, false);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(true, 0, 0);
  CheckTextCellTitle(@"prepopulated-2", 0, 1);
  CheckTextCellChecked(false, 0, 1);

  template_url_service_->SetUserSelectedDefaultSearchProvider(url_2);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(false, 0, 0);
  CheckTextCellTitle(@"prepopulated-2", 0, 1);
  CheckTextCellChecked(true, 0, 1);

  template_url_service_->SetUserSelectedDefaultSearchProvider(url_1);
  template_url_service_->Remove(url_2);

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(true, 0, 0);
}

// Tests that when user change default search engine, all items can be displayed
// correctly and the change can be synced to the prefs.
TEST_F(SearchEngineSettingsCollectionViewControllerTest, TestChangeProvider) {
  TemplateURL* url_1 = AddPriorSearchEngine("prepopulated-1", 1, false);
  TemplateURL* url_2 = AddPriorSearchEngine("prepopulated-2", 2, true);

  CreateController();
  CheckController();

  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(true, 0, 0);
  CheckTextCellTitle(@"prepopulated-2", 0, 1);
  CheckTextCellChecked(false, 0, 1);
  EXPECT_EQ(url_1, template_url_service_->GetDefaultSearchProvider());

  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckTextCellTitle(@"prepopulated-1", 0, 0);
  CheckTextCellChecked(false, 0, 0);
  CheckTextCellTitle(@"prepopulated-2", 0, 1);
  CheckTextCellChecked(true, 0, 1);
  EXPECT_EQ(url_2, template_url_service_->GetDefaultSearchProvider());

  // Check that the selection was written back to the prefs.
  const base::DictionaryValue* searchProviderDict =
      chrome_browser_state_->GetTestingPrefService()->GetDictionary(
          DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ASSERT_TRUE(searchProviderDict);
  base::string16 short_name;
  EXPECT_TRUE(searchProviderDict->GetString(DefaultSearchManager::kShortName,
                                            &short_name));
  EXPECT_EQ(url_2->short_name(), short_name);
}

}  // namespace
