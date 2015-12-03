// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/navigation_message_processor.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

using testing::_;

namespace blimp {

class MockNavigationMessageDelegate
    : public NavigationMessageProcessor::NavigationMessageDelegate {
 public:
  // NavigationMessageDelegate implementation.
  MOCK_METHOD2(OnUrlChanged, void(int tab_id, const GURL& url));
  MOCK_METHOD2(OnFaviconChanged, void(int tab_id, const SkBitmap& favicon));
  MOCK_METHOD2(OnTitleChanged, void(int tab_id, const std::string& title));
  MOCK_METHOD2(OnLoadingChanged, void(int tab_id, bool loading));
};

void SendMockNavigationStateChangedMessage(BlimpMessageProcessor* processor,
                                           int tab_id,
                                           const GURL* url,
                                           const std::string* title,
                                           const bool* loading) {
  scoped_ptr<BlimpMessage> message(new BlimpMessage);
  message->set_type(BlimpMessage::NAVIGATION);
  message->set_target_tab_id(tab_id);

  NavigationMessage* details = message->mutable_navigation();
  details->set_type(NavigationMessage::NAVIGATION_STATE_CHANGED);

  NavigationStateChangeMessage* state =
      details->mutable_navigation_state_change();
  if (url)
    state->set_url(url->spec());

  if (title)
    state->set_title(*title);

  if (loading)
    state->set_loading(*loading);

  processor->ProcessMessage(std::move(message), net::CompletionCallback());
}

MATCHER_P2(EqualsNavigateToUrlText, tab_id, text, "") {
  return arg.target_tab_id() == tab_id &&
      arg.navigation().type() == NavigationMessage::LOAD_URL &&
      arg.navigation().load_url().url() == text;
}

MATCHER_P(EqualsNavigateForward, tab_id, "") {
  return arg.target_tab_id() == tab_id &&
      arg.navigation().type() == NavigationMessage::GO_FORWARD;
}

MATCHER_P(EqualsNavigateBack, tab_id, "") {
  return arg.target_tab_id() == tab_id &&
      arg.navigation().type() == NavigationMessage::GO_BACK;
}

MATCHER_P(EqualsNavigateReload, tab_id, "") {
  return arg.target_tab_id() == tab_id &&
      arg.navigation().type() == NavigationMessage::RELOAD;
}


class NavigationMessageProcessorTest : public testing::Test {
 public:
  NavigationMessageProcessorTest()
      : processor_(&out_processor_) {}

  void SetUp() override {
    processor_.SetDelegate(1, &delegate1_);
    processor_.SetDelegate(2, &delegate2_);
  }

 protected:
  MockBlimpMessageProcessor out_processor_;
  MockNavigationMessageDelegate delegate1_;
  MockNavigationMessageDelegate delegate2_;

  NavigationMessageProcessor processor_;
};

TEST_F(NavigationMessageProcessorTest, DispatchesToCorrectDelegate) {
  GURL url("https://www.google.com");
  EXPECT_CALL(delegate1_, OnUrlChanged(1, url)).Times(1);
  SendMockNavigationStateChangedMessage(
      &processor_, 1, &url, nullptr, nullptr);

  EXPECT_CALL(delegate2_, OnUrlChanged(2, url)).Times(1);
  SendMockNavigationStateChangedMessage(
      &processor_, 2, &url, nullptr, nullptr);
}

TEST_F(NavigationMessageProcessorTest, AllDelegateFieldsCalled) {
  GURL url("https://www.google.com");
  std::string title = "Google";
  bool loading = true;

  EXPECT_CALL(delegate1_, OnUrlChanged(1, url)).Times(1);
  EXPECT_CALL(delegate1_, OnTitleChanged(1, title)).Times(1);
  EXPECT_CALL(delegate1_, OnLoadingChanged(1, loading)).Times(1);
  SendMockNavigationStateChangedMessage(&processor_, 1, &url, &title, &loading);
}

TEST_F(NavigationMessageProcessorTest, PartialDelegateFieldsCalled) {
  std::string title = "Google";
  bool loading = true;

  EXPECT_CALL(delegate1_, OnUrlChanged(_, _)).Times(0);
  EXPECT_CALL(delegate1_, OnTitleChanged(1, title)).Times(1);
  EXPECT_CALL(delegate1_, OnLoadingChanged(1, loading)).Times(1);
  SendMockNavigationStateChangedMessage(
      &processor_, 1, nullptr, &title, &loading);
}

TEST_F(NavigationMessageProcessorTest, TestNavigateToUrlMessage) {
  std::string text = "text";

  EXPECT_CALL(out_processor_,
              MockableProcessMessage(
                  EqualsNavigateToUrlText(1, text), _)).Times(1);
  processor_.NavigateToUrlText(1, text);
}

TEST_F(NavigationMessageProcessorTest, TestNavigateForwardMessage) {
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(EqualsNavigateForward(1), _)).Times(1);
  processor_.GoForward(1);
}

TEST_F(NavigationMessageProcessorTest, TestNavigateBackMessage) {
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(EqualsNavigateBack(1), _)).Times(1);
  processor_.GoBack(1);
}

TEST_F(NavigationMessageProcessorTest, TestNavigateReloadMessage) {
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(EqualsNavigateReload(1), _)).Times(1);
  processor_.Reload(1);
}

}  // namespace blimp
