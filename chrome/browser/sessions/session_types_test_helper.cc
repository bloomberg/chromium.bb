// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sessions/session_types_test_helper.h"

#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sessions/session_types.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"

void SessionTypesTestHelper::ExpectNavigationEquals(
    const TabNavigation& expected,
    const TabNavigation& actual) {
  EXPECT_EQ(expected.referrer_.url, actual.referrer_.url);
  EXPECT_EQ(expected.referrer_.policy, actual.referrer_.policy);
  EXPECT_EQ(expected.virtual_url_, actual.virtual_url_);
  EXPECT_EQ(expected.title_, actual.title_);
  EXPECT_EQ(expected.content_state_, actual.content_state_);
  EXPECT_EQ(expected.transition_type_, actual.transition_type_);
  EXPECT_EQ(expected.has_post_data_, actual.has_post_data_);
  EXPECT_EQ(expected.original_request_url_, actual.original_request_url_);
  EXPECT_EQ(expected.is_overriding_user_agent_,
            actual.is_overriding_user_agent_);
}

TabNavigation SessionTypesTestHelper::CreateNavigation(
    const std::string& virtual_url,
    const std::string& title) {
  TabNavigation navigation;
  navigation.index_ = 0;
  navigation.referrer_ =
      content::Referrer(GURL("http://www.referrer.com"),
                        WebKit::WebReferrerPolicyDefault);
  navigation.virtual_url_ = GURL(virtual_url);
  navigation.title_ = UTF8ToUTF16(title);
  navigation.content_state_ = "fake_state";
  navigation.timestamp_ = base::Time::Now();
  return navigation;
}

const content::Referrer& SessionTypesTestHelper::GetReferrer(
    const TabNavigation& navigation) {
  return navigation.referrer_;
}

content::PageTransition SessionTypesTestHelper::GetTransitionType(
    const TabNavigation& navigation) {
  return navigation.transition_type_;
}

bool SessionTypesTestHelper::GetHasPostData(const TabNavigation& navigation) {
  return navigation.has_post_data_;
}

int64 SessionTypesTestHelper::GetPostID(const TabNavigation& navigation) {
  return navigation.post_id_;
}

const GURL& SessionTypesTestHelper::GetOriginalRequestURL(
    const TabNavigation& navigation) {
  return navigation.original_request_url_;
}

bool SessionTypesTestHelper::GetIsOverridingUserAgent(
    const TabNavigation& navigation) {
  return navigation.is_overriding_user_agent_;
}

base::Time SessionTypesTestHelper::GetTimestamp(
    const TabNavigation& navigation) {
  return navigation.timestamp_;
}

void SessionTypesTestHelper::SetContentState(
    TabNavigation* navigation,
    const std::string& content_state) {
  navigation->content_state_ = content_state;
}

void SessionTypesTestHelper::SetHasPostData(TabNavigation* navigation,
                                            bool has_post_data) {
  navigation->has_post_data_ = has_post_data;
}

void SessionTypesTestHelper::SetOriginalRequestURL(
    TabNavigation* navigation,
    const GURL& original_request_url) {
  navigation->original_request_url_ = original_request_url;
}

void SessionTypesTestHelper::SetIsOverridingUserAgent(
    TabNavigation* navigation,
    bool is_overriding_user_agent) {
  navigation->is_overriding_user_agent_ = is_overriding_user_agent;
}

void SessionTypesTestHelper::SetTimestamp(
    TabNavigation* navigation,
    base::Time timestamp) {
  navigation->timestamp_ = timestamp;
}
