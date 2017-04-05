// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_

#import "base/ios/block_types.h"
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

  // Loads the specified HTML content with URL into the WebState.
  void LoadHtml(NSString* html, const GURL& url);
  // Loads the specified HTML content into the WebState, using test url name.
  void LoadHtml(NSString* html);
  // Loads the specified HTML content into the WebState, using test url name.
  void LoadHtml(const std::string& html);
  // Blocks until both known NSRunLoop-based and known message-loop-based
  // background tasks have completed
  void WaitForBackgroundTasks();
  // Blocks until known NSRunLoop-based have completed, known message-loop-based
  // background tasks have completed and |condition| evaluates to true.
  void WaitForCondition(ConditionBlock condition);
  // Synchronously executes JavaScript and returns result as id.
  id ExecuteJavaScript(NSString* script);
  // Destroys underlying WebState. web_state() will return null after this call.
  void DestroyWebState();

  // Returns the base URL of the loaded page.
  std::string BaseUrl() const;

  // Returns web state for this web controller.
  web::WebState* web_state();
  const web::WebState* web_state() const;

 private:
  // base::MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // The web state for testing.
  std::unique_ptr<WebState> web_state_;
  // true if a task has been processed.
  bool processed_a_task_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_WITH_WEB_STATE_H_
