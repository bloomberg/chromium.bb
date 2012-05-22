// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_client.h"

#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

class PushClientTest : public testing::Test {
 protected:
  PushClientTest() {
    notifier_options_.request_context_getter =
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy());
  }

  virtual ~PushClientTest() {}

  // The sockets created by the XMPP code expect an IO loop.
  MessageLoopForIO message_loop_;
  NotifierOptions notifier_options_;
};

// Make sure calling CreateDefault on the IO thread doesn't blow up.
TEST_F(PushClientTest, OnIOThread) {
  const scoped_ptr<PushClient> push_client(
      PushClient::CreateDefault(notifier_options_));
}

// Make sure calling CreateDefault on a non-IO thread doesn't blow up.
TEST_F(PushClientTest, OffIOThread) {
  base::Thread thread("Non-IO thread");
  EXPECT_TRUE(thread.Start());
  thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&PushClient::CreateDefault),
                 notifier_options_));
  thread.Stop();
}

}  // namespace

}  // namespace notifier
