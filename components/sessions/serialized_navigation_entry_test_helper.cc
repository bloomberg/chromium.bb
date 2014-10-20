// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/serialized_navigation_entry_test_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/common/page_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "url/gurl.h"

namespace sessions {

// static
void SerializedNavigationEntryTestHelper::ExpectNavigationEquals(
    const SerializedNavigationEntry& expected,
    const SerializedNavigationEntry& actual) {
  EXPECT_EQ(expected.referrer_url_, actual.referrer_url_);
  EXPECT_EQ(expected.referrer_policy_, actual.referrer_policy_);
  EXPECT_EQ(expected.virtual_url_, actual.virtual_url_);
  EXPECT_EQ(expected.title_, actual.title_);
  EXPECT_EQ(expected.encoded_page_state_, actual.encoded_page_state_);
  EXPECT_EQ(expected.transition_type_, actual.transition_type_);
  EXPECT_EQ(expected.has_post_data_, actual.has_post_data_);
  EXPECT_EQ(expected.original_request_url_, actual.original_request_url_);
  EXPECT_EQ(expected.is_overriding_user_agent_,
            actual.is_overriding_user_agent_);
}

// static
SerializedNavigationEntry SerializedNavigationEntryTestHelper::CreateNavigation(
    const std::string& virtual_url,
    const std::string& title) {
  SerializedNavigationEntry navigation;
  navigation.index_ = 0;
  navigation.referrer_url_ = GURL("http://www.referrer.com");
  navigation.referrer_policy_ = blink::WebReferrerPolicyDefault;
  navigation.virtual_url_ = GURL(virtual_url);
  navigation.title_ = base::UTF8ToUTF16(title);
  navigation.encoded_page_state_ = "fake state";
  navigation.timestamp_ = base::Time::Now();
  navigation.http_status_code_ = 200;
  return navigation;
}

// static
void SerializedNavigationEntryTestHelper::SetPageState(
    const content::PageState& page_state,
    SerializedNavigationEntry* navigation) {
  navigation->encoded_page_state_ = page_state.ToEncodedData();
}

// static
void SerializedNavigationEntryTestHelper::SetHasPostData(
    bool has_post_data,
    SerializedNavigationEntry* navigation) {
  navigation->has_post_data_ = has_post_data;
}

// static
void SerializedNavigationEntryTestHelper::SetOriginalRequestURL(
    const GURL& original_request_url,
    SerializedNavigationEntry* navigation) {
  navigation->original_request_url_ = original_request_url;
}

// static
void SerializedNavigationEntryTestHelper::SetIsOverridingUserAgent(
    bool is_overriding_user_agent,
    SerializedNavigationEntry* navigation) {
  navigation->is_overriding_user_agent_ = is_overriding_user_agent;
}

// static
void SerializedNavigationEntryTestHelper::SetTimestamp(
    base::Time timestamp,
    SerializedNavigationEntry* navigation) {
  navigation->timestamp_ = timestamp;
}

}  // namespace sessions
