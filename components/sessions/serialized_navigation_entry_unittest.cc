// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/serialized_navigation_entry.h"

#include <cstddef>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "url/gurl.h"

namespace sessions {
namespace {

const int kIndex = 3;
const int kUniqueID = 50;
const content::Referrer kReferrer =
    content::Referrer(GURL("http://www.referrer.com"),
                      blink::WebReferrerPolicyAlways);
const GURL kVirtualURL("http://www.virtual-url.com");
const base::string16 kTitle = base::ASCIIToUTF16("title");
const content::PageState kPageState =
    content::PageState::CreateFromEncodedData("page state");
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
const base::string16 kSearchTerms = base::ASCIIToUTF16("my search terms");
const GURL kFaviconURL("http://virtual-url.com/favicon.ico");
const int kHttpStatusCode = 404;
const GURL kRedirectURL0("http://go/redirect0");
const GURL kRedirectURL1("http://go/redirect1");
const GURL kOtherURL("http://other.com");

const int kPageID = 10;

// Create a NavigationEntry from the constants above.
scoped_ptr<content::NavigationEntry> MakeNavigationEntryForTest() {
  scoped_ptr<content::NavigationEntry> navigation_entry(
      content::NavigationEntry::Create());
  navigation_entry->SetReferrer(kReferrer);
  navigation_entry->SetVirtualURL(kVirtualURL);
  navigation_entry->SetTitle(kTitle);
  navigation_entry->SetPageState(kPageState);
  navigation_entry->SetTransitionType(kTransitionType);
  navigation_entry->SetHasPostData(kHasPostData);
  navigation_entry->SetPostID(kPostID);
  navigation_entry->SetOriginalRequestURL(kOriginalRequestURL);
  navigation_entry->SetIsOverridingUserAgent(kIsOverridingUserAgent);
  navigation_entry->SetTimestamp(kTimestamp);
  navigation_entry->SetExtraData(kSearchTermsKey, kSearchTerms);
  navigation_entry->GetFavicon().valid = true;
  navigation_entry->GetFavicon().url = kFaviconURL;
  navigation_entry->SetHttpStatusCode(kHttpStatusCode);
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(kRedirectURL0);
  redirect_chain.push_back(kRedirectURL1);
  redirect_chain.push_back(kVirtualURL);
  navigation_entry->SetRedirectChain(redirect_chain);
  return navigation_entry.Pass();
}

// Create a sync_pb::TabNavigation from the constants above.
sync_pb::TabNavigation MakeSyncDataForTest() {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url(kVirtualURL.spec());
  sync_data.set_referrer(kReferrer.url.spec());
  sync_data.set_referrer_policy(blink::WebReferrerPolicyOrigin);
  sync_data.set_title(base::UTF16ToUTF8(kTitle));
  sync_data.set_state(kPageState.ToEncodedData());
  sync_data.set_page_transition(
      sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME);
  sync_data.set_unique_id(kUniqueID);
  sync_data.set_timestamp_msec(syncer::TimeToProtoTime(kTimestamp));
  sync_data.set_redirect_type(sync_pb::SyncEnums::CLIENT_REDIRECT);
  sync_data.set_navigation_home_page(true);
  sync_data.set_search_terms(base::UTF16ToUTF8(kSearchTerms));
  sync_data.set_favicon_url(kFaviconURL.spec());
  sync_data.set_http_status_code(kHttpStatusCode);
  // The redirect chain only syncs one way.
  return sync_data;
}

// Create a default SerializedNavigationEntry.  All its fields should be
// initialized to their respective default values.
TEST(SerializedNavigationEntryTest, DefaultInitializer) {
  const SerializedNavigationEntry navigation;
  EXPECT_EQ(-1, navigation.index());
  EXPECT_EQ(0, navigation.unique_id());
  EXPECT_EQ(GURL(), navigation.referrer().url);
  EXPECT_EQ(blink::WebReferrerPolicyDefault, navigation.referrer().policy);
  EXPECT_EQ(GURL(), navigation.virtual_url());
  EXPECT_TRUE(navigation.title().empty());
  EXPECT_FALSE(navigation.page_state().IsValid());
  EXPECT_EQ(content::PAGE_TRANSITION_TYPED, navigation.transition_type());
  EXPECT_FALSE(navigation.has_post_data());
  EXPECT_EQ(-1, navigation.post_id());
  EXPECT_EQ(GURL(), navigation.original_request_url());
  EXPECT_FALSE(navigation.is_overriding_user_agent());
  EXPECT_TRUE(navigation.timestamp().is_null());
  EXPECT_TRUE(navigation.search_terms().empty());
  EXPECT_FALSE(navigation.favicon_url().is_valid());
  EXPECT_EQ(0, navigation.http_status_code());
  EXPECT_EQ(0U, navigation.redirect_chain().size());
}

// Create a SerializedNavigationEntry from a NavigationEntry.  All its fields
// should match the NavigationEntry's.
TEST(SerializedNavigationEntryTest, FromNavigationEntry) {
  const scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const SerializedNavigationEntry& navigation =
      SerializedNavigationEntry::FromNavigationEntry(kIndex, *navigation_entry);

  EXPECT_EQ(kIndex, navigation.index());

  EXPECT_EQ(navigation_entry->GetUniqueID(), navigation.unique_id());
  EXPECT_EQ(kReferrer.url, navigation.referrer().url);
  EXPECT_EQ(kReferrer.policy, navigation.referrer().policy);
  EXPECT_EQ(kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(kTitle, navigation.title());
  EXPECT_EQ(kPageState, navigation.page_state());
  EXPECT_EQ(kTransitionType, navigation.transition_type());
  EXPECT_EQ(kHasPostData, navigation.has_post_data());
  EXPECT_EQ(kPostID, navigation.post_id());
  EXPECT_EQ(kOriginalRequestURL, navigation.original_request_url());
  EXPECT_EQ(kIsOverridingUserAgent, navigation.is_overriding_user_agent());
  EXPECT_EQ(kTimestamp, navigation.timestamp());
  EXPECT_EQ(kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(kHttpStatusCode, navigation.http_status_code());
  ASSERT_EQ(3U, navigation.redirect_chain().size());
  EXPECT_EQ(kRedirectURL0, navigation.redirect_chain()[0]);
  EXPECT_EQ(kRedirectURL1, navigation.redirect_chain()[1]);
  EXPECT_EQ(kVirtualURL, navigation.redirect_chain()[2]);
}

// Create a SerializedNavigationEntry from a sync_pb::TabNavigation.  All its
// fields should match the protocol buffer's if it exists there, and
// sbould be set to the default value otherwise.
TEST(SerializedNavigationEntryTest, FromSyncData) {
  const sync_pb::TabNavigation sync_data = MakeSyncDataForTest();

  const SerializedNavigationEntry& navigation =
      SerializedNavigationEntry::FromSyncData(kIndex, sync_data);

  EXPECT_EQ(kIndex, navigation.index());
  EXPECT_EQ(kUniqueID, navigation.unique_id());
  EXPECT_EQ(kReferrer.url, navigation.referrer().url);
  EXPECT_EQ(blink::WebReferrerPolicyOrigin, navigation.referrer().policy);
  EXPECT_EQ(kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(kTitle, navigation.title());
  EXPECT_EQ(kPageState, navigation.page_state());
  EXPECT_EQ(kTransitionType, navigation.transition_type());
  EXPECT_FALSE(navigation.has_post_data());
  EXPECT_EQ(-1, navigation.post_id());
  EXPECT_EQ(GURL(), navigation.original_request_url());
  EXPECT_FALSE(navigation.is_overriding_user_agent());
  EXPECT_TRUE(navigation.timestamp().is_null());
  EXPECT_EQ(kSearchTerms, navigation.search_terms());
  EXPECT_EQ(kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(kHttpStatusCode, navigation.http_status_code());
  // The redirect chain only syncs one way.
}

// Create a SerializedNavigationEntry, pickle it, then create another one by
// unpickling.  The new one should match the old one except for fields
// that aren't pickled, which should be set to default values.
TEST(SerializedNavigationEntryTest, Pickle) {
  const SerializedNavigationEntry& old_navigation =
      SerializedNavigationEntry::FromNavigationEntry(
          kIndex, *MakeNavigationEntryForTest());

  Pickle pickle;
  old_navigation.WriteToPickle(30000, &pickle);

  SerializedNavigationEntry new_navigation;
  PickleIterator pickle_iterator(pickle);
  EXPECT_TRUE(new_navigation.ReadFromPickle(&pickle_iterator));

  EXPECT_EQ(kIndex, new_navigation.index());

  EXPECT_EQ(0, new_navigation.unique_id());
  EXPECT_EQ(kReferrer.url, new_navigation.referrer().url);
  EXPECT_EQ(kReferrer.policy, new_navigation.referrer().policy);
  EXPECT_EQ(kVirtualURL, new_navigation.virtual_url());
  EXPECT_EQ(kTitle, new_navigation.title());
  EXPECT_FALSE(new_navigation.page_state().IsValid());
  EXPECT_EQ(kTransitionType, new_navigation.transition_type());
  EXPECT_EQ(kHasPostData, new_navigation.has_post_data());
  EXPECT_EQ(-1, new_navigation.post_id());
  EXPECT_EQ(kOriginalRequestURL, new_navigation.original_request_url());
  EXPECT_EQ(kIsOverridingUserAgent, new_navigation.is_overriding_user_agent());
  EXPECT_EQ(kTimestamp, new_navigation.timestamp());
  EXPECT_EQ(kSearchTerms, new_navigation.search_terms());
  EXPECT_EQ(kHttpStatusCode, new_navigation.http_status_code());
  EXPECT_EQ(0U, new_navigation.redirect_chain().size());
}

// Create a NavigationEntry, then create another one by converting to
// a SerializedNavigationEntry and back.  The new one should match the old one
// except for fields that aren't preserved, which should be set to
// expected values.
TEST(SerializedNavigationEntryTest, ToNavigationEntry) {
  const scoped_ptr<content::NavigationEntry> old_navigation_entry(
      MakeNavigationEntryForTest());

  const SerializedNavigationEntry& navigation =
      SerializedNavigationEntry::FromNavigationEntry(kIndex,
                                                     *old_navigation_entry);

  const scoped_ptr<content::NavigationEntry> new_navigation_entry(
      navigation.ToNavigationEntry(kPageID, NULL));

  EXPECT_EQ(kReferrer.url, new_navigation_entry->GetReferrer().url);
  EXPECT_EQ(kReferrer.policy, new_navigation_entry->GetReferrer().policy);
  EXPECT_EQ(kVirtualURL, new_navigation_entry->GetVirtualURL());
  EXPECT_EQ(kTitle, new_navigation_entry->GetTitle());
  EXPECT_EQ(kPageState, new_navigation_entry->GetPageState());
  EXPECT_EQ(kPageID, new_navigation_entry->GetPageID());
  EXPECT_EQ(content::PAGE_TRANSITION_RELOAD,
            new_navigation_entry->GetTransitionType());
  EXPECT_EQ(kHasPostData, new_navigation_entry->GetHasPostData());
  EXPECT_EQ(kPostID, new_navigation_entry->GetPostID());
  EXPECT_EQ(kOriginalRequestURL,
            new_navigation_entry->GetOriginalRequestURL());
  EXPECT_EQ(kIsOverridingUserAgent,
            new_navigation_entry->GetIsOverridingUserAgent());
  base::string16 search_terms;
  new_navigation_entry->GetExtraData(kSearchTermsKey, &search_terms);
  EXPECT_EQ(kSearchTerms, search_terms);
  EXPECT_EQ(kHttpStatusCode, new_navigation_entry->GetHttpStatusCode());
  ASSERT_EQ(3U, new_navigation_entry->GetRedirectChain().size());
  EXPECT_EQ(kRedirectURL0, new_navigation_entry->GetRedirectChain()[0]);
  EXPECT_EQ(kRedirectURL1, new_navigation_entry->GetRedirectChain()[1]);
  EXPECT_EQ(kVirtualURL, new_navigation_entry->GetRedirectChain()[2]);
}

// Create a NavigationEntry, convert it to a SerializedNavigationEntry, then
// create a sync protocol buffer from it.  The protocol buffer should
// have matching fields to the NavigationEntry (when applicable).
TEST(SerializedNavigationEntryTest, ToSyncData) {
  const scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const SerializedNavigationEntry& navigation =
      SerializedNavigationEntry::FromNavigationEntry(kIndex, *navigation_entry);

  const sync_pb::TabNavigation sync_data = navigation.ToSyncData();

  EXPECT_EQ(kVirtualURL.spec(), sync_data.virtual_url());
  EXPECT_EQ(kReferrer.url.spec(), sync_data.referrer());
  EXPECT_EQ(kTitle, base::ASCIIToUTF16(sync_data.title()));
  EXPECT_TRUE(sync_data.state().empty());
  EXPECT_EQ(sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME,
            sync_data.page_transition());
  EXPECT_TRUE(sync_data.has_redirect_type());
  EXPECT_EQ(navigation_entry->GetUniqueID(), sync_data.unique_id());
  EXPECT_EQ(syncer::TimeToProtoTime(kTimestamp), sync_data.timestamp_msec());
  EXPECT_EQ(kTimestamp.ToInternalValue(), sync_data.global_id());
  EXPECT_EQ(kFaviconURL.spec(), sync_data.favicon_url());
  EXPECT_EQ(kHttpStatusCode, sync_data.http_status_code());
  // The proto navigation redirects don't include the final chain entry
  // (because it didn't redirect) so the lengths should differ by 1.
  ASSERT_EQ(3, sync_data.navigation_redirect_size() + 1);
  EXPECT_EQ(navigation_entry->GetRedirectChain()[0].spec(),
            sync_data.navigation_redirect(0).url());
  EXPECT_EQ(navigation_entry->GetRedirectChain()[1].spec(),
            sync_data.navigation_redirect(1).url());
  EXPECT_FALSE(sync_data.has_last_navigation_redirect_url());
}

// Test that the last_navigation_redirect_url is set when needed.
// This test is just like the above, but with a different virtual_url.
// Create a NavigationEntry, convert it to a SerializedNavigationEntry, then
// create a sync protocol buffer from it.  The protocol buffer should
// have a last_navigation_redirect_url.
TEST(SerializedNavigationEntryTest, LastNavigationRedirectUrl) {
  const scoped_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  navigation_entry->SetVirtualURL(kOtherURL);

  const SerializedNavigationEntry& navigation =
      SerializedNavigationEntry::FromNavigationEntry(kIndex, *navigation_entry);

  const sync_pb::TabNavigation sync_data = navigation.ToSyncData();

  EXPECT_TRUE(sync_data.has_last_navigation_redirect_url());
  EXPECT_EQ(kVirtualURL.spec(), sync_data.last_navigation_redirect_url());

  // The redirect chain should be the same as in the above test.
  ASSERT_EQ(3, sync_data.navigation_redirect_size() + 1);
  EXPECT_EQ(navigation_entry->GetRedirectChain()[0].spec(),
            sync_data.navigation_redirect(0).url());
  EXPECT_EQ(navigation_entry->GetRedirectChain()[1].spec(),
            sync_data.navigation_redirect(1).url());
}

// Ensure all transition types and qualifiers are converted to/from the sync
// SerializedNavigationEntry representation properly.
TEST(SerializedNavigationEntryTest, TransitionTypes) {
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
      const SerializedNavigationEntry& navigation =
          SerializedNavigationEntry::FromNavigationEntry(kIndex,
                                                         *navigation_entry);
      const sync_pb::TabNavigation& sync_data = navigation.ToSyncData();
      const SerializedNavigationEntry& constructed_nav =
          SerializedNavigationEntry::FromSyncData(kIndex, sync_data);
      const content::PageTransition constructed_transition =
          constructed_nav.transition_type();

      EXPECT_EQ(transition, constructed_transition);
    }
  }
}

// Tests that the input data is sanitized when a SerializedNavigationEntry
// is created from a pickle format.
TEST(SerializedNavigationEntryTest, Sanitize) {
  sync_pb::TabNavigation sync_data = MakeSyncDataForTest();

  sync_data.set_referrer_policy(blink::WebReferrerPolicyNever);
  content::PageState page_state =
      content::PageState::CreateFromURL(kVirtualURL);
  sync_data.set_state(page_state.ToEncodedData());

  const SerializedNavigationEntry& navigation =
      SerializedNavigationEntry::FromSyncData(kIndex, sync_data);

  EXPECT_EQ(kIndex, navigation.index());
  EXPECT_EQ(kUniqueID, navigation.unique_id());
  EXPECT_EQ(GURL(), navigation.referrer().url);
  EXPECT_EQ(blink::WebReferrerPolicyDefault, navigation.referrer().policy);
  EXPECT_EQ(kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(kTitle, navigation.title());
  EXPECT_EQ(page_state, navigation.page_state());
  EXPECT_EQ(kTransitionType, navigation.transition_type());
  EXPECT_FALSE(navigation.has_post_data());
  EXPECT_EQ(-1, navigation.post_id());
  EXPECT_EQ(GURL(), navigation.original_request_url());
  EXPECT_FALSE(navigation.is_overriding_user_agent());
  EXPECT_TRUE(navigation.timestamp().is_null());
  EXPECT_EQ(kSearchTerms, navigation.search_terms());
  EXPECT_EQ(kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(kHttpStatusCode, navigation.http_status_code());

  content::PageState empty_state;
  EXPECT_TRUE(empty_state.Equals(empty_state.RemoveReferrer()));
}

}  // namespace
}  // namespace sessions
