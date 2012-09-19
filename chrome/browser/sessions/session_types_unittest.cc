// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/session_types_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"

namespace {

const int kIndex = 3;
const int kUniqueID = 50;
const content::Referrer kReferrer =
    content::Referrer(GURL("http://www.referrer.com"),
                      WebKit::WebReferrerPolicyAlways);
const GURL kVirtualURL("http://www.virtual-url.com");
const string16 kTitle = ASCIIToUTF16("title");
const std::string kContentState = "content state";
const content::PageTransition kTransitionType =
    content::PAGE_TRANSITION_AUTO_SUBFRAME;
const bool kHasPostData = true;
const int64 kPostID = 100;
const GURL kOriginalRequestURL("http://www.original-request.com");
const bool kIsOverridingUserAgent = true;
const base::Time kTimestamp = syncer::ProtoTimeToTime(100);

const int kPageID = 10;

// Create a NavigationEntry from the constants above.
scoped_ptr<content::NavigationEntry> MakeNavigationEntryForTest() {
  scoped_ptr<content::NavigationEntry> navigation_entry(
      content::NavigationEntry::Create());
  navigation_entry->SetReferrer(kReferrer);
  navigation_entry->SetVirtualURL(kVirtualURL);
  navigation_entry->SetTitle(kTitle);
  navigation_entry->SetContentState(kContentState);
  navigation_entry->SetTransitionType(kTransitionType);
  navigation_entry->SetHasPostData(kHasPostData);
  navigation_entry->SetPostID(kPostID);
  navigation_entry->SetOriginalRequestURL(kOriginalRequestURL);
  navigation_entry->SetIsOverridingUserAgent(kIsOverridingUserAgent);
  return navigation_entry.Pass();
}

// Create a sync_pb::TabNavigation from the constants above.
sync_pb::TabNavigation MakeSyncDataForTest() {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url(kVirtualURL.spec());
  sync_data.set_referrer(kReferrer.url.spec());
  sync_data.set_title(UTF16ToUTF8(kTitle));
  sync_data.set_state(kContentState);
  sync_data.set_page_transition(
      sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME);
  sync_data.set_unique_id(kUniqueID);
  sync_data.set_timestamp(syncer::TimeToProtoTime(kTimestamp));
  return sync_data;
}

// Create a default TabNavigation.  All its fields should be
// initialized to their respective default values.
TEST(TabNavigationTest, DefaultInitializer) {
  const TabNavigation navigation;
  EXPECT_EQ(-1, navigation.index());
  EXPECT_EQ(0, navigation.unique_id());
  EXPECT_EQ(GURL(), SessionTypesTestHelper::GetReferrer(navigation).url);
  EXPECT_EQ(WebKit::WebReferrerPolicyDefault,
            SessionTypesTestHelper::GetReferrer(navigation).policy);
  EXPECT_EQ(GURL(), navigation.virtual_url());
  EXPECT_TRUE(navigation.title().empty());
  EXPECT_TRUE(navigation.content_state().empty());
  EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
            SessionTypesTestHelper::GetTransitionType(navigation));
  EXPECT_FALSE(SessionTypesTestHelper::GetHasPostData(navigation));
  EXPECT_EQ(-1, SessionTypesTestHelper::GetPostID(navigation));
  EXPECT_EQ(GURL(), SessionTypesTestHelper::GetOriginalRequestURL(navigation));
  EXPECT_FALSE(SessionTypesTestHelper::GetIsOverridingUserAgent(navigation));
  EXPECT_EQ(base::Time(), navigation.timestamp());
}

// Create a TabNavigation from a NavigationEntry.  All its fields
// should match the NavigationEntry's.
TEST(TabNavigationTest, FromNavigationEntry) {
  const scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const TabNavigation& navigation =
      TabNavigation::FromNavigationEntry(
          kIndex, *navigation_entry, kTimestamp);

  EXPECT_EQ(kIndex, navigation.index());

  EXPECT_EQ(navigation_entry->GetUniqueID(), navigation.unique_id());
  EXPECT_EQ(kReferrer.url,
            SessionTypesTestHelper::GetReferrer(navigation).url);
  EXPECT_EQ(kReferrer.policy,
            SessionTypesTestHelper::GetReferrer(navigation).policy);
  EXPECT_EQ(kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(kTitle, navigation.title());
  EXPECT_EQ(kContentState, navigation.content_state());
  EXPECT_EQ(kTransitionType,
            SessionTypesTestHelper::GetTransitionType(navigation));
  EXPECT_EQ(kHasPostData, SessionTypesTestHelper::GetHasPostData(navigation));
  EXPECT_EQ(kPostID, SessionTypesTestHelper::GetPostID(navigation));
  EXPECT_EQ(kOriginalRequestURL,
            SessionTypesTestHelper::GetOriginalRequestURL(navigation));
  EXPECT_EQ(kIsOverridingUserAgent,
            SessionTypesTestHelper::GetIsOverridingUserAgent(navigation));
  EXPECT_EQ(kTimestamp, navigation.timestamp());
}

// Create a TabNavigation from a sync_pb::TabNavigation.  All its
// fields should match the protocol buffer's if it exists there, and
// sbould be set to the default value otherwise.
TEST(TabNavigationTest, FromSyncData) {
  const sync_pb::TabNavigation sync_data = MakeSyncDataForTest();

  const TabNavigation& navigation =
      TabNavigation::FromSyncData(kIndex, sync_data);

  EXPECT_EQ(kIndex, navigation.index());
  EXPECT_EQ(kUniqueID, navigation.unique_id());
  EXPECT_EQ(kReferrer.url,
            SessionTypesTestHelper::GetReferrer(navigation).url);
  EXPECT_EQ(WebKit::WebReferrerPolicyDefault,
            SessionTypesTestHelper::GetReferrer(navigation).policy);
  EXPECT_EQ(kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(kTitle, navigation.title());
  EXPECT_EQ(kContentState, navigation.content_state());
  EXPECT_EQ(kTransitionType,
            SessionTypesTestHelper::GetTransitionType(navigation));
  EXPECT_FALSE(SessionTypesTestHelper::GetHasPostData(navigation));
  EXPECT_EQ(-1, SessionTypesTestHelper::GetPostID(navigation));
  EXPECT_EQ(GURL(), SessionTypesTestHelper::GetOriginalRequestURL(navigation));
  EXPECT_FALSE(SessionTypesTestHelper::GetIsOverridingUserAgent(navigation));
  EXPECT_EQ(kTimestamp, navigation.timestamp());
}

// Create a TabNavigation, pickle it, then create another one by
// unpickling.  The new one should match the old one except for fields
// that aren't pickled, which should be set to default values.
TEST(TabNavigationTest, Pickle) {
  const TabNavigation& old_navigation =
      TabNavigation::FromNavigationEntry(
          kIndex, *MakeNavigationEntryForTest(), kTimestamp);

  Pickle pickle;
  old_navigation.WriteToPickle(&pickle);

  TabNavigation new_navigation;
  PickleIterator pickle_iterator(pickle);
  EXPECT_TRUE(new_navigation.ReadFromPickle(&pickle_iterator));

  EXPECT_EQ(kIndex, new_navigation.index());

  EXPECT_EQ(0, new_navigation.unique_id());
  EXPECT_EQ(kReferrer.url,
            SessionTypesTestHelper::GetReferrer(new_navigation).url);
  EXPECT_EQ(kReferrer.policy,
            SessionTypesTestHelper::GetReferrer(new_navigation).policy);
  EXPECT_EQ(kVirtualURL, new_navigation.virtual_url());
  EXPECT_EQ(kTitle, new_navigation.title());
  EXPECT_TRUE(new_navigation.content_state().empty());
  EXPECT_EQ(kTransitionType,
            SessionTypesTestHelper::GetTransitionType(new_navigation));
  EXPECT_EQ(kHasPostData,
            SessionTypesTestHelper::GetHasPostData(new_navigation));
  EXPECT_EQ(-1, SessionTypesTestHelper::GetPostID(new_navigation));
  EXPECT_EQ(kOriginalRequestURL,
            SessionTypesTestHelper::GetOriginalRequestURL(new_navigation));
  EXPECT_EQ(kIsOverridingUserAgent,
            SessionTypesTestHelper::GetIsOverridingUserAgent(new_navigation));
  EXPECT_EQ(base::Time(), new_navigation.timestamp());
}

// Create a NavigationEntry, then create another one by converting to
// a TabNavigation and back.  The new one should match the old one
// except for fields that aren't preserved, which should be set to
// expected values.
TEST(TabNavigationTest, ToNavigationEntry) {
  const scoped_ptr<content::NavigationEntry> old_navigation_entry(
      MakeNavigationEntryForTest());

  const TabNavigation& navigation =
      TabNavigation::FromNavigationEntry(
          kIndex, *old_navigation_entry, kTimestamp);

  const scoped_ptr<content::NavigationEntry> new_navigation_entry(
      navigation.ToNavigationEntry(kPageID, NULL));

  EXPECT_EQ(kReferrer.url, new_navigation_entry->GetReferrer().url);
  EXPECT_EQ(kReferrer.policy, new_navigation_entry->GetReferrer().policy);
  EXPECT_EQ(kVirtualURL, new_navigation_entry->GetVirtualURL());
  EXPECT_EQ(kTitle, new_navigation_entry->GetTitle());
  EXPECT_EQ(kContentState, new_navigation_entry->GetContentState());
  EXPECT_EQ(kPageID, new_navigation_entry->GetPageID());
  EXPECT_EQ(content::PAGE_TRANSITION_RELOAD,
            new_navigation_entry->GetTransitionType());
  EXPECT_EQ(kHasPostData, new_navigation_entry->GetHasPostData());
  EXPECT_EQ(kPostID, new_navigation_entry->GetPostID());
  EXPECT_EQ(kOriginalRequestURL,
            new_navigation_entry->GetOriginalRequestURL());
  EXPECT_EQ(kIsOverridingUserAgent,
            new_navigation_entry->GetIsOverridingUserAgent());
}

// Create a NavigationEntry, convert it to a TabNavigation, then
// create a sync protocol buffer from it.  The protocol buffer should
// have matching fields to the NavigationEntry (when applicable).
TEST(TabNavigationTest, ToSyncData) {
  const scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const TabNavigation& navigation =
      TabNavigation::FromNavigationEntry(
          kIndex, *navigation_entry, kTimestamp);

  const sync_pb::TabNavigation sync_data = navigation.ToSyncData();

  EXPECT_EQ(kVirtualURL.spec(), sync_data.virtual_url());
  EXPECT_EQ(kReferrer.url.spec(), sync_data.referrer());
  EXPECT_EQ(kTitle, ASCIIToUTF16(sync_data.title()));
  EXPECT_TRUE(sync_data.state().empty());
  EXPECT_EQ(sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME,
            sync_data.page_transition());
  EXPECT_FALSE(sync_data.has_navigation_qualifier());
  EXPECT_EQ(navigation_entry->GetUniqueID(), sync_data.unique_id());
  EXPECT_EQ(syncer::TimeToProtoTime(kTimestamp), sync_data.timestamp());
}

}  // namespace
