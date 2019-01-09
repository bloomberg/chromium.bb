// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_TEST_FAKES_FAKE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_DELEGATE_H_

#include <memory>

#include "ios/chrome/browser/web/java_script_console/java_script_console_message.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_tab_helper_delegate.h"

// A JavaScriptConsoleTabHelperDelegate class which stores the last received
// message from |DidReceiveConsoleMessage|.
class FakeJavaScriptConsoleTabHelperDelegate
    : public JavaScriptConsoleTabHelperDelegate {
 public:
  FakeJavaScriptConsoleTabHelperDelegate();
  ~FakeJavaScriptConsoleTabHelperDelegate() override;

  void DidReceiveConsoleMessage(
      web::WebState* web_state,
      web::WebFrame* sender_frame,
      const JavaScriptConsoleMessage& message) override;

  // Returns the last messaged logged.
  const JavaScriptConsoleMessage* GetLastLoggedMessage() const;

 private:
  // The last received message.
  std::unique_ptr<JavaScriptConsoleMessage> last_received_message_;
};

#endif  // IOS_CHROME_TEST_FAKES_FAKE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_DELEGATE_H_
