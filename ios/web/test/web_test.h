// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WEB_TEST_H_
#define IOS_WEB_TEST_WEB_TEST_H_

#import <UIKit/UIKit.h>

#import "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_ui_web_view_web_controller.h"
#include "testing/platform_test.h"

// A subclass of WebController overridden for testing purposes.  Specifically it
// overrides the UIWebView delegate method to intercept requests coming from
// core.js.
// TODO(jimblackler): remove use of TestWebController entirely.
@interface TestWebController : CRWUIWebViewWebController

@property(nonatomic, assign) BOOL interceptRequest;
@property(nonatomic, assign) BOOL requestIntercepted;
@property(nonatomic, assign) BOOL
    invokeShouldStartLoadWithRequestNavigationTypeDone;

@end

namespace web {

// An abstract class of tests that need to create real WebControllers that can
// be loaded with test HTML and JavaScripts. Concrete subclasses override
// |CreateWebController| specifying the test WebController object.
class WebTestBase : public PlatformTest,
                    public base::MessageLoop::TaskObserver {
 public:
  ~WebTestBase() override;

 protected:
  WebTestBase();
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
  // Returns true if WebController message queue is empty.
  // |WaitForBackgroundTasks| does not return until until the message queue is
  // empty.
  bool MessageQueueIsEmpty() const;
  // Evaluates JavaScript and returns result as a string.
  NSString* EvaluateJavaScriptAsString(NSString* script) const;
  // Runs the given JavaScript and returns the result as a string. This method
  // is a drop-in replacement for stringByEvaluatingJavaScriptFromString with
  // the additional functionality that any JavaScript exceptions are caught and
  // logged (not dropped silently).
  NSString* RunJavaScript(NSString* script);
  // Returns a CRWWebController to be used in tests.
  virtual CRWWebController* CreateWebController() = 0;
  // TaskObserver methods (used when waiting for background tasks).
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // The threads used for testing.
  web::TestWebThreadBundle thread_bundle_;
  // The web controller for testing.
  base::scoped_nsobject<CRWWebController> webController_;
  // true if a task has been processed.
  bool processed_a_task_;
  // The WebClient used in tests.
  scoped_ptr<web::WebClient> client_;
  // The browser state used in tests.
  TestBrowserState browser_state_;
};

#pragma mark -

// A concrete test class that is needed to create a real
// CRWUIWebViewWebController.
class UIWebViewWebTest : public WebTestBase {
 protected:
  CRWWebController* CreateWebController() override;
  // Invokes JS->ObjC messages directly on the web controller, registering a
  // human interaction if userIsInteraction==YES. |commands| should be a
  // stringified message queue.
  void LoadCommands(NSString* commands,
                    const GURL& origin_url,
                    BOOL user_is_interacting);
};

#pragma mark -

// A concrete test class that is needed to create a real
// CRWWKWebViewWebController.
class WKWebViewWebTest : public WebTestBase {
 protected:
  CRWWebController* CreateWebController() override;
};

}  // namespace web

#endif // IOS_WEB_TEST_WEB_TEST_H_
