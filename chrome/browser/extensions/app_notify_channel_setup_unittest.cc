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
  TestDelegate() : was_called_(false) {}
  virtual ~TestDelegate() {}

  virtual void AppNotifyChannelSetupComplete(
      const std::string& channel_id,
      const std::string& error,
      int route_id,
      int callback_id) OVERRIDE {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_FALSE(was_called_);
    was_called_ = true;
    error_ = error;
    route_id_ = route_id;
    callback_id_ = callback_id;
    MessageLoop::current()->Quit();
  }

  // Called to check that we were called with the expected arguments.
  void ExpectWasCalled(const std::string& expected_channel_id,
                       const std::string& expected_error,
                       int expected_route_id,
                       int expected_callback_id) {
    EXPECT_TRUE(was_called_);
    EXPECT_EQ(expected_error, error_);
    EXPECT_EQ(expected_route_id, route_id_);
    EXPECT_EQ(expected_callback_id, callback_id_);
  }

 private:
  // Has our callback been called yet?
  bool was_called_;

  // When our AppNotifyChannelSetupComplete method is called, we copy the
  // arguments into these member variables.
  std::string channel_id_;
  std::string error_;
  int route_id_;
  int callback_id_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace


TEST(AppNotifyChannelSetupTest, WasDelegateCalled) {
  MessageLoop message_loop;
  BrowserThread thread(BrowserThread::UI, &message_loop);

  TestDelegate delegate;
  std::string client_id = "12345";
  int route_id = 4;
  int callback_id = 5;
  GURL url("http://www.google.com");

  scoped_refptr<AppNotifyChannelSetup > setup =
      new AppNotifyChannelSetup(client_id,
                                url,
                                route_id,
                                callback_id,
                                delegate.AsWeakPtr());
  setup->Start();
  MessageLoop::current()->Run();
  delegate.ExpectWasCalled(
      std::string(), std::string("not_implemented"), route_id, callback_id);
}
