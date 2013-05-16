// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_suite.h"

#include "content/shell/common/shell_content_client.h"
#include "content/shell/shell_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/message_loop.h"
#include "content/test/browser_test_message_pump_android.h"
#endif

namespace content {

namespace {

class ContentShellTestSuiteInitializer
    : public testing::EmptyTestEventListener {
 public:
  ContentShellTestSuiteInitializer() {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    content_client_.reset(new ShellContentClient);
    browser_content_client_.reset(new ShellContentBrowserClient());
    SetContentClient(content_client_.get());
    SetBrowserClientForTesting(browser_content_client_.get());
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    browser_content_client_.reset();
    content_client_.reset();
    SetContentClient(NULL);
  }

 private:
  scoped_ptr<ShellContentClient> content_client_;
  scoped_ptr<ShellContentBrowserClient> browser_content_client_;

  DISALLOW_COPY_AND_ASSIGN(ContentShellTestSuiteInitializer);
};

#if defined(OS_ANDROID)
base::MessagePump* CreateMessagePumpForUI() {
  return new BrowserTestMessagePumpAndroid();
}
#endif

}  // namespace

ContentBrowserTestSuite::ContentBrowserTestSuite(int argc, char** argv)
    : ContentTestSuiteBase(argc, argv) {
}

ContentBrowserTestSuite::~ContentBrowserTestSuite() {
}

void ContentBrowserTestSuite::Initialize() {
#if defined(OS_ANDROID)
  // This needs to be done before base::TestSuite::Initialize() is called,
  // as it also tries to set MessagePumpForUIFactory.
  if (!MessageLoop::InitMessagePumpForUIFactory(&CreateMessagePumpForUI))
    LOG(INFO) << "MessagePumpForUIFactory already set, unable to override.";
#endif

  ContentTestSuiteBase::Initialize();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ContentShellTestSuiteInitializer);
}

ContentClient* ContentBrowserTestSuite::CreateClientForInitialization() {
  return new ShellContentClient();
}

}  // namespace content
