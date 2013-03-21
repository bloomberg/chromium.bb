// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/session_types_test_helper.h"
#include "content/public/browser/favicon_status.h"
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
    static_cast<content::PageTransition>(
        content::PAGE_TRANSITION_AUTO_SUBFRAME |
        content::PAGE_TRANSITION_HOME_PAGE |
        content::PAGE_TRANSITION_CLIENT_REDIRECT);
const bool kHasPostData = true;
const int64 kPostID = 100;
const GURL kOriginalRequestURL("http://www.original-request.com");
const bool kIsOverridingUserAgent = true;
const base::Time kTimestamp = syncer::ProtoTimeToTime(100);
const string16 kSearchTerms = ASCIIToUTF16("my search terms");
const GURL kFaviconURL("http://virtual-url.com/favicon.ico");

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
  navigation_entry->SetTimestamp(kTimestamp);
  navigation_entry->SetExtraData(
      chrome::search::kInstantExtendedSearchTermsKey, kSearchTerms);
  navigation_entry->GetFavicon().valid = true;
  navigation_entry->GetFavicon().url = kFaviconURL;
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
  sync_data.set_timestamp_msec(syncer::TimeToProtoTime(kTimestamp));
  sync_data.set_redirect_type(sync_pb::SyncEnums::CLIENT_REDIRECT);
  sync_data.set_navigation_home_page(true);
  sync_data.set_search_terms(UTF16ToUTF8(kSearchTerms));
  sync_data.set_favicon_url(kFaviconURL.spec());
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
  EXPECT_TRUE(SessionTypesTestHelper::GetTimestamp(navigation).is_null());
  EXPECT_TRUE(navigation.search_terms().empty());
  EXPECT_FALSE(navigation.favicon_url().is_valid());
}

// Create a TabNavigation from a NavigationEntry.  All its fields
// should match the NavigationEntry's.
TEST(TabNavigationTest, FromNavigationEntry) {
  const scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const TabNavigation& navigation =
      TabNavigation::FromNavigationEntry(kIndex, *navigation_entry);

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
  EXPECT_EQ(kTimestamp, SessionTypesTestHelper::GetTimestamp(navigation));
  EXPECT_EQ(kFaviconURL, navigation.favicon_url());
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
  EXPECT_TRUE(SessionTypesTestHelper::GetTimestamp(navigation).is_null());
  EXPECT_EQ(kSearchTerms, navigation.search_terms());
  EXPECT_EQ(kFaviconURL, navigation.favicon_url());
}

// Create a TabNavigation, pickle it, then create another one by
// unpickling.  The new one should match the old one except for fields
// that aren't pickled, which should be set to default values.
TEST(TabNavigationTest, Pickle) {
  const TabNavigation& old_navigation =
      TabNavigation::FromNavigationEntry(kIndex, *MakeNavigationEntryForTest());

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
  EXPECT_EQ(kTimestamp, SessionTypesTestHelper::GetTimestamp(new_navigation));
  EXPECT_EQ(kSearchTerms, new_navigation.search_terms());
}

// Create a NavigationEntry, then create another one by converting to
// a TabNavigation and back.  The new one should match the old one
// except for fields that aren't preserved, which should be set to
// expected values.
TEST(TabNavigationTest, ToNavigationEntry) {
  const scoped_ptr<content::NavigationEntry> old_navigation_entry(
      MakeNavigationEntryForTest());

  const TabNavigation& navigation =
      TabNavigation::FromNavigationEntry(kIndex, *old_navigation_entry);

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
  EXPECT_EQ(kSearchTerms,
            chrome::search::GetSearchTermsFromNavigationEntry(
                new_navigation_entry.get()));
}

// Create a NavigationEntry, convert it to a TabNavigation, then
// create a sync protocol buffer from it.  The protocol buffer should
// have matching fields to the NavigationEntry (when applicable).
TEST(TabNavigationTest, ToSyncData) {
  const scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const TabNavigation& navigation =
      TabNavigation::FromNavigationEntry(kIndex, *navigation_entry);

  const sync_pb::TabNavigation sync_data = navigation.ToSyncData();

  EXPECT_EQ(kVirtualURL.spec(), sync_data.virtual_url());
  EXPECT_EQ(kReferrer.url.spec(), sync_data.referrer());
  EXPECT_EQ(kTitle, ASCIIToUTF16(sync_data.title()));
  EXPECT_TRUE(sync_data.state().empty());
  EXPECT_EQ(sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME,
            sync_data.page_transition());
  EXPECT_TRUE(sync_data.has_redirect_type());
  EXPECT_EQ(navigation_entry->GetUniqueID(), sync_data.unique_id());
  EXPECT_EQ(syncer::TimeToProtoTime(kTimestamp), sync_data.timestamp_msec());
  EXPECT_EQ(kTimestamp.ToInternalValue(), sync_data.global_id());
  EXPECT_EQ(kFaviconURL.spec(), sync_data.favicon_url());
}

// Ensure all transition types and qualifiers are converted to/from the sync
// TabNavigation representation properly.
TEST(TabNavigationTest, TransitionTypes) {
  scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());
  for (uint32 core_type = content::PAGE_TRANSITION_LINK;
       core_type != content::PAGE_TRANSITION_LAST_CORE; ++core_type) {
    // Because qualifier is a uint32, left shifting will eventually overflow
    // and hit zero again. SERVER_REDIRECT, as the last qualifier and also
    // in place of the sign bit, is therefore the last transition before
    // breaking.
    for (uint32 qualifier = content::PAGE_TRANSITION_FORWARD_BACK;
         qualifier != 0; qualifier <<= 1) {
      if (qualifier == 0x08000000)
        continue;  // 0x08000000 is not a valid qualifier.
      content::PageTransition transition =
          static_cast<content::PageTransition>(core_type | qualifier);

      navigation_entry->SetTransitionType(transition);
      const TabNavigation& navigation =
          TabNavigation::FromNavigationEntry(kIndex, *navigation_entry);
      const sync_pb::TabNavigation& sync_data = navigation.ToSyncData();
      const TabNavigation& constructed_nav =
          TabNavigation::FromSyncData(kIndex, sync_data);
      const content::PageTransition constructed_transition =
          SessionTypesTestHelper::GetTransitionType(constructed_nav);

      EXPECT_EQ(transition, constructed_transition);
    }
  }
}

// Create a typical SessionTab protocol buffer and set an existing
// SessionTab from it.  The data from the protocol buffer should
// clobber the existing data.
TEST(SessionTab, FromSyncData) {
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_id(5);
  sync_data.set_window_id(10);
  sync_data.set_tab_visual_index(13);
  sync_data.set_current_navigation_index(3);
  sync_data.set_pinned(true);
  sync_data.set_extension_app_id("app_id");
  for (int i = 0; i < 5; ++i) {
    sync_pb::TabNavigation* navigation = sync_data.add_navigation();
    navigation->set_virtual_url("http://foo/" + base::IntToString(i));
    navigation->set_referrer("referrer");
    navigation->set_title("title");
    navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
  }

  SessionTab tab;
  tab.window_id.set_id(100);
  tab.tab_id.set_id(100);
  tab.tab_visual_index = 100;
  tab.current_navigation_index = 1000;
  tab.pinned = false;
  tab.extension_app_id = "fake";
  tab.user_agent_override = "fake";
  tab.timestamp = base::Time::FromInternalValue(100);
  tab.navigations.resize(100);
  tab.session_storage_persistent_id = "fake";

  tab.SetFromSyncData(sync_data, base::Time::FromInternalValue(5u));
  EXPECT_EQ(10, tab.window_id.id());
  EXPECT_EQ(5, tab.tab_id.id());
  EXPECT_EQ(13, tab.tab_visual_index);
  EXPECT_EQ(3, tab.current_navigation_index);
  EXPECT_TRUE(tab.pinned);
  EXPECT_EQ("app_id", tab.extension_app_id);
  EXPECT_TRUE(tab.user_agent_override.empty());
  EXPECT_EQ(5u, tab.timestamp.ToInternalValue());
  ASSERT_EQ(5u, tab.navigations.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(i, tab.navigations[i].index());
    EXPECT_EQ(GURL("referrer"),
              SessionTypesTestHelper::GetReferrer(tab.navigations[i]).url);
    EXPECT_EQ(string16(ASCIIToUTF16("title")), tab.navigations[i].title());
    EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
              SessionTypesTestHelper::GetTransitionType(tab.navigations[i]));
    EXPECT_EQ(GURL("http://foo/" + base::IntToString(i)),
              tab.navigations[i].virtual_url());
  }
  EXPECT_TRUE(tab.session_storage_persistent_id.empty());
}

TEST(SessionTab, ToSyncData) {
  SessionTab tab;
  tab.window_id.set_id(10);
  tab.tab_id.set_id(5);
  tab.tab_visual_index = 13;
  tab.current_navigation_index = 3;
  tab.pinned = true;
  tab.extension_app_id = "app_id";
  tab.user_agent_override = "fake";
  tab.timestamp = base::Time::FromInternalValue(100);
  for (int i = 0; i < 5; ++i) {
    tab.navigations.push_back(
        SessionTypesTestHelper::CreateNavigation(
            "http://foo/" + base::IntToString(i), "title"));
  }
  tab.session_storage_persistent_id = "fake";

  const sync_pb::SessionTab& sync_data = tab.ToSyncData();
  EXPECT_EQ(5, sync_data.tab_id());
  EXPECT_EQ(10, sync_data.window_id());
  EXPECT_EQ(13, sync_data.tab_visual_index());
  EXPECT_EQ(3, sync_data.current_navigation_index());
  EXPECT_TRUE(sync_data.pinned());
  EXPECT_EQ("app_id", sync_data.extension_app_id());
  ASSERT_EQ(5, sync_data.navigation_size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(tab.navigations[i].virtual_url().spec(),
              sync_data.navigation(i).virtual_url());
    EXPECT_EQ(UTF16ToUTF8(tab.navigations[i].title()),
              sync_data.navigation(i).title());
  }
  EXPECT_FALSE(sync_data.has_favicon());
  EXPECT_FALSE(sync_data.has_favicon_type());
  EXPECT_FALSE(sync_data.has_favicon_source());
}

}  // namespace
