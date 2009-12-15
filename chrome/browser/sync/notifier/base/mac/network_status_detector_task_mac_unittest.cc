// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/mac/network_status_detector_task_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include "talk/base/messagequeue.h"
#include "talk/base/sigslot.h"
#include "talk/base/taskrunner.h"
#include "talk/base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

// No anonymous namespace because we use FRIEND_TESTs.

class NetworkStatusDetectorTaskMacTest : public testing::Test {
};

// TODO(akalin): We can't test much with the current interface.
// Extend it so we're able to inject mock network events and then add
// more tests.

// Some basic sanity checks to make sure the object is destroyed
// cleanly with various configurations.

TEST_F(NetworkStatusDetectorTaskMacTest, InitTest) {
  NetworkStatusDetectorTaskMac network_status_detector_mac(NULL);
}

TEST_F(NetworkStatusDetectorTaskMacTest, StartNoStopTest) {
  NetworkStatusDetectorTaskMac network_status_detector_mac(NULL);
  EXPECT_EQ(NetworkStatusDetectorTaskMac::STATE_RESPONSE,
            network_status_detector_mac.ProcessStart());
}

class DummyTaskRunner : public talk_base::TaskRunner {
 public:
  virtual void WakeTasks() {}
  virtual int64 CurrentTime() { return 0; }
};

TEST_F(NetworkStatusDetectorTaskMacTest, StartStopTest) {
  DummyTaskRunner task_runner;
  NetworkStatusDetectorTaskMac network_status_detector_mac(&task_runner);
  EXPECT_EQ(NetworkStatusDetectorTaskMac::STATE_RESPONSE,
            network_status_detector_mac.ProcessStart());
  network_status_detector_mac.Stop();
}

// Some miscellaneous tests.

class AliveListener : public sigslot::has_slots<> {
 public:
  AliveListener()
      : was_alive_(false),
        is_alive_(false),
        set_alive_called_(false) {}

  void SetAlive(bool was_alive, bool is_alive) {
    was_alive_ = was_alive;
    is_alive_ = is_alive;
    set_alive_called_ = true;
  }

  void ResetSetAliveCalled() {
    set_alive_called_ = false;
  }

  bool was_alive() const { return was_alive_; }
  bool is_alive() const { return is_alive_; }
  bool set_alive_called() const { return set_alive_called_; }

 private:
  bool was_alive_, is_alive_, set_alive_called_;
};

TEST_F(NetworkStatusDetectorTaskMacTest, OnMessageTest) {
  NetworkStatusDetectorTaskMac network_status_detector_mac(NULL);
  AliveListener alive_listener;
  network_status_detector_mac.SignalNetworkStateDetected.connect(
      &alive_listener, &AliveListener::SetAlive);

  talk_base::Message message;

  alive_listener.ResetSetAliveCalled();
  message.message_id = 0;
  network_status_detector_mac.OnMessage(&message);
  EXPECT_TRUE(alive_listener.set_alive_called());
  EXPECT_FALSE(alive_listener.was_alive());
  EXPECT_FALSE(alive_listener.is_alive());

  alive_listener.ResetSetAliveCalled();
  message.message_id = 5;
  network_status_detector_mac.OnMessage(&message);
  EXPECT_TRUE(alive_listener.set_alive_called());
  EXPECT_FALSE(alive_listener.was_alive());
  EXPECT_TRUE(alive_listener.is_alive());

  alive_listener.ResetSetAliveCalled();
  message.message_id = 0;
  network_status_detector_mac.OnMessage(&message);
  EXPECT_TRUE(alive_listener.set_alive_called());
  EXPECT_TRUE(alive_listener.was_alive());
  EXPECT_FALSE(alive_listener.is_alive());
}

}  // namespace notifier

