// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry_loading_util.h"

#include "base/memory/ptr_util.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

// A mock NetworkChangeNotifier that will report the network state passed in the
// constructor.
class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  MockNetworkChangeNotifier(ConnectionType connection)
      : NetworkChangeNotifier() {
    connection_ = connection;
  }

  ~MockNetworkChangeNotifier() override {}

  ConnectionType GetCurrentConnectionType() const override {
    return connection_;
  };

 private:
  ConnectionType connection_;
};

// Test fixture to test loading of Reading list entries.
typedef web::WebTestWithWebState ReadingListEntryLoadingUtilTest;

// Tests that loading a not distilled entry with network will load online
// version.
TEST_F(ReadingListEntryLoadingUtilTest, TestLoadEntryOnlineWODistilled) {
  MockNetworkChangeNotifier network_enabler(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  GURL url("http://foo.bar");
  auto reading_list_model =
      base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr);
  reading_list_model->AddEntry(url, "title");
  const ReadingListEntry* entry = reading_list_model->GetEntryByURL(url);
  reading_list::LoadReadingListEntry(*entry, reading_list_model.get(),
                                     web_state());
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  EXPECT_EQ(navigation_manager->GetPendingItem()->GetURL(), url);
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading a distilled entry with network will load online version.
TEST_F(ReadingListEntryLoadingUtilTest, TestLoadEntryOnlineWithistilled) {
  MockNetworkChangeNotifier network_enabler(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  GURL url("http://foo.bar");
  std::string distilled_path = "distilled/page.html";
  auto reading_list_model =
      base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr);
  reading_list_model->AddEntry(url, "title");
  reading_list_model->SetEntryDistilledPath(url,
                                            base::FilePath(distilled_path));
  const ReadingListEntry* entry = reading_list_model->GetEntryByURL(url);
  reading_list::LoadReadingListEntry(*entry, reading_list_model.get(),
                                     web_state());
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  EXPECT_EQ(navigation_manager->GetPendingItem()->GetURL(), url);
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading a not distilled entry without network will load online
// version.
TEST_F(ReadingListEntryLoadingUtilTest, TestLoadEntryOfflineWODistilled) {
  MockNetworkChangeNotifier network_disabler(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  GURL url("http://foo.bar");
  auto reading_list_model =
      base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr);
  reading_list_model->AddEntry(url, "title");
  const ReadingListEntry* entry = reading_list_model->GetEntryByURL(url);
  reading_list::LoadReadingListEntry(*entry, reading_list_model.get(),
                                     web_state());
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  EXPECT_EQ(navigation_manager->GetPendingItem()->GetURL(), url);
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading a distilled entry without network will load offline
// version.
TEST_F(ReadingListEntryLoadingUtilTest, TestLoadEntryOfflineWithDistilled) {
  MockNetworkChangeNotifier network_disabler(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  GURL url("http://foo.bar");
  std::string distilled_path = "distilled/page.html";
  auto reading_list_model =
      base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr);
  reading_list_model->AddEntry(url, "title");
  reading_list_model->SetEntryDistilledPath(url,
                                            base::FilePath(distilled_path));
  const ReadingListEntry* entry = reading_list_model->GetEntryByURL(url);
  reading_list::LoadReadingListEntry(*entry, reading_list_model.get(),
                                     web_state());
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  EXPECT_NE(navigation_manager->GetPendingItem()->GetURL(), url);
  EXPECT_EQ(
      navigation_manager->GetPendingItem()->GetURL(),
      reading_list::DistilledURLForPath(entry->DistilledPath(), entry->URL()));
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading a distilled version of an entry.
TEST_F(ReadingListEntryLoadingUtilTest, TestLoadReadingListDistilled) {
  GURL url("http://foo.bar");
  std::string distilled_path = "distilled/page.html";
  auto reading_list_model =
      base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr);
  reading_list_model->AddEntry(url, "title");
  reading_list_model->SetEntryDistilledPath(url,
                                            base::FilePath(distilled_path));
  const ReadingListEntry* entry = reading_list_model->GetEntryByURL(url);
  reading_list::LoadReadingListDistilled(*entry, reading_list_model.get(),
                                         web_state());
  web::NavigationManager* navigation_manager =
      web_state()->GetNavigationManager();
  EXPECT_NE(navigation_manager->GetPendingItem()->GetURL(), url);
  EXPECT_EQ(
      navigation_manager->GetPendingItem()->GetURL(),
      reading_list::DistilledURLForPath(entry->DistilledPath(), entry->URL()));
  EXPECT_TRUE(entry->IsRead());
}
