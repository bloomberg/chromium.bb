// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WEB_TEST_H_
#define IOS_WEB_TEST_WEB_TEST_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#include "testing/platform_test.h"

namespace web {

// A test fixture for web tests that need a minimum environment set up that
// mimics a web embedder.
class WebTest : public PlatformTest {
 protected:
  WebTest();
  ~WebTest() override;

  // PlatformTest methods.
  void SetUp() override;
  void TearDown() override;

  // Returns the WebClient that is used for testing.
  TestWebClient* GetWebClient();

  // Returns the BrowserState that is used for testing.
  virtual BrowserState* GetBrowserState();

 private:
  // The WebClient used in tests.
  ScopedTestingWebClient web_client_;
  // The threads used for testing.
  web::TestWebThreadBundle thread_bundle_;
  // The browser state used in tests.
  TestBrowserState browser_state_;
};

#pragma mark -

// An abstract test fixture that sets up a WebControllers that can be loaded
// with test HTML and JavaScripts.
class WebTestWithWebController : public WebTest,
                                 public base::MessageLoop::TaskObserver {
 public:
  ~WebTestWithWebController() override;

 protected:
  WebTestWithWebController();
  void SetUp() override;
  void TearDown() override;
  // Loads the specified HTML content into the WebController via public APIs.
  void LoadHtml(NSString* html);
  // Loads the specified HTML content into the WebController via public APIs.
  void LoadHtml(const std::string& html);
  // Loads |url| into the WebController via public APIs.
  // Note if anyone uses this to load web pages from the live internet, the
  // tests can be flaky / dependent on content and behavior beyond our control.
  // Use this only when it's impossible to test with static HTML using LoadHtml.
  void LoadURL(const GURL& url);
  // Blocks until both known NSRunLoop-based and known message-loop-based
  // background tasks have completed
  void WaitForBackgroundTasks();
  // Blocks until known NSRunLoop-based have completed, known message-loop-based
  // background tasks have completed and |condition| evaluates to true.
  void WaitForCondition(ConditionBlock condition);
  // Evaluates JavaScript and returns result as a string.
  // DEPRECATED. TODO(crbug.com/595761): Remove this API.
  NSString* EvaluateJavaScriptAsString(NSString* script);
  // Synchronously executes JavaScript and returns result as id.
  id ExecuteJavaScript(NSString* script);
  // TaskObserver methods (used when waiting for background tasks).
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // The web controller for testing.
  base::WeakNSObject<CRWWebController> webController_;
  // true if a task has been processed.
  bool processed_a_task_;

 private:
  // LoadURL() for data URLs sometimes lock up navigation, so if the loaded page
  // is not the one expected, reset the web view. In some cases, document or
  // document.body does not exist either; also reset in those cases.
  // Returns true if a reset occurred. One may want to load the page again.
  bool ResetPageIfNavigationStalled(NSString* load_check);
  // Creates a unique HTML element to look for in
  // ResetPageIfNavigationStalled().
  NSString* CreateLoadCheck();
  // The web state for testing.
  std::unique_ptr<WebStateImpl> web_state_impl_;
};

}  // namespace web

#endif  // IOS_WEB_TEST_WEB_TEST_H_
