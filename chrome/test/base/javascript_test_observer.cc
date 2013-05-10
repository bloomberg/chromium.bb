// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/javascript_test_observer.h"

#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"

TestMessageHandler::TestMessageHandler() : ok_(true) {
}

TestMessageHandler::~TestMessageHandler() {
}

void TestMessageHandler::SetError(const std::string& message) {
  ok_ = false;
  error_message_ = message;
}

void TestMessageHandler::Reset() {
  ok_ = true;
  error_message_.clear();
}

JavascriptTestObserver::JavascriptTestObserver(
    content::RenderViewHost* render_view_host,
    TestMessageHandler* handler)
    : handler_(handler),
      running_(false),
      finished_(false) {
  Reset();
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::Source<content::RenderViewHost>(render_view_host));
}

JavascriptTestObserver::~JavascriptTestObserver() {
}

bool JavascriptTestObserver::Run() {
  // Messages may have arrived before Run was called.
  if (!finished_) {
    CHECK(!running_);
    running_ = true;
    content::RunMessageLoop();
    running_ = false;
  }
  return handler_->ok();
}

void JavascriptTestObserver::Reset() {
  CHECK(!running_);
  running_ = false;
  finished_ = false;
  handler_->Reset();
}

void JavascriptTestObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  CHECK(type == content::NOTIFICATION_DOM_OPERATION_RESPONSE);
  content::Details<content::DomOperationNotificationDetails> dom_op_details(
      details);
  // We might receive responses for other script execution, but we only
  // care about the test finished message.
  TestMessageHandler::MessageResponse response =
      handler_->HandleMessage(dom_op_details->json);

  if (response == TestMessageHandler::DONE) {
    EndTest();
  } else {
    Continue();
  }
}

void JavascriptTestObserver::Continue() {
}

void JavascriptTestObserver::EndTest() {
  finished_ = true;
  if (running_) {
    running_ = false;
    base::MessageLoopForUI::current()->Quit();
  }
}
