// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#include "base/json/json_writer.h"
#include "base/mac/scoped_nsobject.h"
#include "base/values.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/test/crw_fake_web_controller_observer.h"
#import "ios/web/test/web_test.h"
#include "testing/gtest_mac.h"

namespace web {

// Test fixture to test web controller observing.
class CRWWebControllerObserverTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    fake_web_controller_observer_.reset(
        [[CRWFakeWebControllerObserver alloc] initWithCommandPrefix:@"test"]);
    [webController_ addObserver:fake_web_controller_observer_];
  }

  void TearDown() override {
    [webController_ removeObserver:fake_web_controller_observer_];
    fake_web_controller_observer_.reset();
    web::WebTestWithWebController::TearDown();
  }

  base::scoped_nsobject<CRWFakeWebControllerObserver>
      fake_web_controller_observer_;
};

TEST_F(CRWWebControllerObserverTest, PageLoaded) {
  EXPECT_FALSE([fake_web_controller_observer_ pageLoaded]);
  LoadHtml(@"<p></p>");
  EXPECT_TRUE([fake_web_controller_observer_ pageLoaded]);
}

// Tests that web controller receives a JS message from the page.
TEST_F(CRWWebControllerObserverTest, HandleCommand) {
  LoadHtml(@"<p></p>");
  ASSERT_EQ(0U, [fake_web_controller_observer_ commandsReceived].size());
  base::DictionaryValue command;
  command.SetString("command", "test.testMessage");
  std::string message;
  base::JSONWriter::Write(command, &message);
  EvaluateJavaScriptAsString([NSString
      stringWithFormat:@"__gCrWeb.message.invokeOnHost(%s)", message.c_str()]);
  WaitForBackgroundTasks();
  ASSERT_EQ(1U, [fake_web_controller_observer_ commandsReceived].size());
  EXPECT_TRUE(
      [fake_web_controller_observer_ commandsReceived][0]->Equals(&command));
}

// Tests that web controller receives an immediate JS message from the page.
TEST_F(CRWWebControllerObserverTest, HandleImmediateCommand) {
  LoadHtml(@"<p></p>");
  ASSERT_NSNE(@"http://testimmediate#target",
              [webController_ externalRequestWindowName]);
  // The only valid immediate commands are window.unload and externalRequest.
  base::DictionaryValue command;
  command.SetString("command", "externalRequest");
  command.SetString("href", "http://testimmediate");
  command.SetString("target", "target");
  command.SetString("referrerPolicy", "referrerPolicy");
  std::string message;
  base::JSONWriter::Write(command, &message);
  EvaluateJavaScriptAsString(
      [NSString stringWithFormat:@"__gCrWeb.message.invokeOnHostImmediate(%s)",
                                 message.c_str()]);
  WaitForBackgroundTasks();
  ASSERT_NSEQ(@"http://testimmediate#target",
              [webController_ externalRequestWindowName]);
}

// Send a large number of commands and check each one is immediately received.
TEST_F(CRWWebControllerObserverTest, HandleMultipleCommands) {
  LoadHtml(@"<p></p>");

  base::DictionaryValue command;
  command.SetString("command", "test.testMessage");
  int kNumberMessages = 200;
  for (int count = 0; count <= kNumberMessages; count++) {
    std::string message;
    command.SetInteger("number", count);
    base::JSONWriter::Write(command, &message);
    ASSERT_EQ(0U, [fake_web_controller_observer_ commandsReceived].size());
    EvaluateJavaScriptAsString(
        [NSString stringWithFormat:@"__gCrWeb.message.invokeOnHost(%s)",
                                   message.c_str()]);
    WaitForBackgroundTasks();
    ASSERT_EQ(1U, [fake_web_controller_observer_ commandsReceived].size());
    EXPECT_TRUE(
        [fake_web_controller_observer_ commandsReceived][0]->Equals(&command));
    [fake_web_controller_observer_ commandsReceived].clear();
    ASSERT_EQ(0U, [fake_web_controller_observer_ commandsReceived].size());
  }
}

}  // namespace web
