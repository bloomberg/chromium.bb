// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/app_notify_channel_setup.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestDelegate : public AppNotifyChannelSetup::Delegate,
                     public base::SupportsWeakPtr<TestDelegate> {
 public:
  TestDelegate() was_called_(false) {}
  virtual ~TestDelegate() {}

  virtual void AppNotifyChannelSetupComplete(
      int request_id,
      const std::string& channel_id,
      const std::string& error) OVERRIDE {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_FALSE(was_called_);
    was_called_ = true;
    request_id_ = request_id;
    channel_id_ = channel_id;
    error_ = error;
    MessageLoop::current()->Quit();
  }

  // Called to check that we were called with the expected arguments.
  void ExpectWasCalled(int expected_request_id,
                       const std::string& expected_channel_id,
                       const std::string& expected_error) {
    EXPECT_TRUE(was_called_);
    EXPECT_EQ(expected_request_id, request_id_);
    EXPECT_EQ(expected_channel_id, channel_id_);
    EXPECT_EQ(expected_error, error_);
  }

 private:
  // Has our callback been called yet?
  bool was_called_;

  // When our AppNotifyChannelSetupComplete method is called, we copy the
  // arguments into these member variables.
  int request_id_;
  std::string channel_id_;
  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace


TEST(AppNotifyChannelSetupTest, WasDelegateCalled) {
  MessageLoop message_loop;
  BrowserThread thread(BrowserThread::UI, &message_loop);

  TestDelegate delegate;
  int request_id = 5;
  std::string client_id = "12345";
  GURL url("http://www.google.com");

  scoped_refptr<AppNotifyChannelSetup > setup =
      new AppNotifyChannelSetup(request_id,
                                client_id,
                                url,
                                delegate.AsWeakPtr());
  setup->Start();
  MessageLoop::current()->Run();
  delegate.ExpectWasCalled(
      request_id, std::string(), std::string("not_implemented"));
}
