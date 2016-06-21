// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_

#include "base/ios/block_types.h"
#import "base/ios/weak_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "ios/web/public/test/web_test.h"
#include "url/gurl.h"

namespace web {

class WebState;

// Base test fixture that provides WebState for testing.
class WebTestWithWebState : public WebTest,
                            public base::MessageLoop::TaskObserver {
 protected:
  WebTestWithWebState();
  ~WebTestWithWebState() override;

  // WebTest overrides.
  void SetUp() override;
  void TearDown() override;

  // base::MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

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

  // Returns web state for this web controller.
  web::WebState* web_state();
  const web::WebState* web_state() const;

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
  std::unique_ptr<WebState> web_state_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
