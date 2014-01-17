// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/state_serializer.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace android_webview {

TEST(AndroidWebViewStateSerializerTest, TestHeaderSerialization) {
  Pickle pickle;
  bool result = internal::WriteHeaderToPickle(&pickle);
  EXPECT_TRUE(result);

  PickleIterator iterator(pickle);
  result = internal::RestoreHeaderFromPickle(&iterator);
  EXPECT_TRUE(result);
}

TEST(AndroidWebViewStateSerializerTest, TestNavigationEntrySerialization) {
  // This is required for NavigationEntry::Create.
  content::ContentClient content_client;
  content::SetContentClient(&content_client);
  content::ContentBrowserClient browser_client;
  content::SetBrowserClientForTesting(&browser_client);

  scoped_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());

  const GURL url("http://url");
  const GURL virtual_url("http://virtual_url");
  content::Referrer referrer;
  referrer.url = GURL("http://referrer_url");
  referrer.policy = blink::WebReferrerPolicyOrigin;
  const base::string16 title(base::UTF8ToUTF16("title"));
  const content::PageState page_state =
      content::PageState::CreateFromEncodedData("completely bogus state");
  const bool has_post_data = true;
  const GURL original_request_url("http://original_request_url");
  const GURL base_url_for_data_url("http://base_url");
  const bool is_overriding_user_agent = true;
  const base::Time timestamp = base::Time::FromInternalValue(12345);
  const int http_status_code = 404;

  entry->SetURL(url);
  entry->SetVirtualURL(virtual_url);
  entry->SetReferrer(referrer);
  entry->SetTitle(title);
  entry->SetPageState(page_state);
  entry->SetHasPostData(has_post_data);
  entry->SetOriginalRequestURL(original_request_url);
  entry->SetBaseURLForDataURL(base_url_for_data_url);
  entry->SetIsOverridingUserAgent(is_overriding_user_agent);
  entry->SetTimestamp(timestamp);
  entry->SetHttpStatusCode(http_status_code);

  Pickle pickle;
  bool result = internal::WriteNavigationEntryToPickle(*entry, &pickle);
  EXPECT_TRUE(result);

  scoped_ptr<content::NavigationEntry> copy(content::NavigationEntry::Create());
  PickleIterator iterator(pickle);
  result = internal::RestoreNavigationEntryFromPickle(&iterator, copy.get());
  EXPECT_TRUE(result);

  EXPECT_EQ(url, copy->GetURL());
  EXPECT_EQ(virtual_url, copy->GetVirtualURL());
  EXPECT_EQ(referrer.url, copy->GetReferrer().url);
  EXPECT_EQ(referrer.policy, copy->GetReferrer().policy);
  EXPECT_EQ(title, copy->GetTitle());
  EXPECT_EQ(page_state, copy->GetPageState());
  EXPECT_EQ(has_post_data, copy->GetHasPostData());
  EXPECT_EQ(original_request_url, copy->GetOriginalRequestURL());
  EXPECT_EQ(base_url_for_data_url, copy->GetBaseURLForDataURL());
  EXPECT_EQ(is_overriding_user_agent, copy->GetIsOverridingUserAgent());
  EXPECT_EQ(timestamp, copy->GetTimestamp());
  EXPECT_EQ(http_status_code, copy->GetHttpStatusCode());
}

}  // namespace android_webview
