// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_v8_tracing_bindings.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "net/test/event_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class MojoProxyResolverV8TracingBindingsTest : public testing::Test {
 public:
  enum Event {
    EVENT_ALERT,
    EVENT_ERROR,
  };
  MojoProxyResolverV8TracingBindingsTest() : weak_factory_(this) {}

  void SetUp() override {
    bindings_.reset(new MojoProxyResolverV8TracingBindings<
        MojoProxyResolverV8TracingBindingsTest>(weak_factory_.GetWeakPtr()));
  }

  void Alert(const mojo::String& message) {
    alerts_.push_back(message.To<std::string>());
    waiter_.NotifyEvent(EVENT_ALERT);
  }

  void OnError(int32_t line_number, const mojo::String& message) {
    errors_.push_back(std::make_pair(line_number, message.To<std::string>()));
    waiter_.NotifyEvent(EVENT_ERROR);
  }

  void ResolveDns(interfaces::HostResolverRequestInfoPtr request_info,
                  interfaces::HostResolverRequestClientPtr client) {}

  void SendAlertAndError() {
    bindings_->Alert(base::ASCIIToUTF16("alert"));
    bindings_->OnError(-1, base::ASCIIToUTF16("error"));
  }

 protected:
  scoped_ptr<MojoProxyResolverV8TracingBindings<
      MojoProxyResolverV8TracingBindingsTest>> bindings_;

  EventWaiter<Event> waiter_;

  std::vector<std::string> alerts_;
  std::vector<std::pair<int, std::string>> errors_;

  base::WeakPtrFactory<MojoProxyResolverV8TracingBindingsTest> weak_factory_;
};

TEST_F(MojoProxyResolverV8TracingBindingsTest, Basic) {
  SendAlertAndError();

  EXPECT_TRUE(bindings_->GetHostResolver());
  EXPECT_FALSE(bindings_->GetBoundNetLog().net_log());

  ASSERT_EQ(1u, alerts_.size());
  EXPECT_EQ("alert", alerts_[0]);
  ASSERT_EQ(1u, errors_.size());
  EXPECT_EQ(-1, errors_[0].first);
  EXPECT_EQ("error", errors_[0].second);
}

TEST_F(MojoProxyResolverV8TracingBindingsTest, CalledFromOtherThread) {
  base::Thread thread("alert and error thread");
  thread.Start();
  thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MojoProxyResolverV8TracingBindingsTest::SendAlertAndError,
                 base::Unretained(this)));

  waiter_.WaitForEvent(EVENT_ERROR);

  ASSERT_EQ(1u, alerts_.size());
  EXPECT_EQ("alert", alerts_[0]);
  ASSERT_EQ(1u, errors_.size());
  EXPECT_EQ(-1, errors_[0].first);
  EXPECT_EQ("error", errors_[0].second);
}

}  // namespace net
