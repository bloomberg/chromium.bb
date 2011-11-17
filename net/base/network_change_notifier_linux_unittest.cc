// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;

class NetworkChangeNotifierLinuxTest : public testing::Test {
 protected:
  // A subset of the NetworkManager-defined constants used in
  // the tests below. See network_change_notifier_linux.cc
  // for the full list.
  enum {
    NM_STATE_DISCONNECTED = 20,
    NM_STATE_DISCONNECTING = 30,
    NM_STATE_CONNECTED_SITE = 70,
    NM_STATE_CONNECTED_GLOBAL = 70
  };

  NetworkChangeNotifierLinuxTest()
      : initialized_(false, false) {}

  virtual void SetUp() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    mock_object_proxy_ = new dbus::MockObjectProxy(mock_bus_.get(),
                                                   "service_name",
                                                   "service_path");
    EXPECT_CALL(*mock_bus_, GetObjectProxy(_, _))
        .WillOnce(Return(mock_object_proxy_.get()));

    EXPECT_CALL(*mock_object_proxy_, CallMethod(_, _, _))
        .WillOnce(SaveArg<2>(&response_callback_));
    EXPECT_CALL(*mock_object_proxy_, ConnectToSignal(_, _, _, _))
        .WillOnce(
            DoAll(
                SaveArg<2>(&signal_callback_),
                InvokeWithoutArgs(
                    this,
                    &NetworkChangeNotifierLinuxTest::Initialize)));

    notifier_.reset(NetworkChangeNotifierLinux::CreateForTest(mock_bus_.get()));

    initialized_.Wait();
  }

  void Initialize() {
    notifier_thread_proxy_ = base::MessageLoopProxy::current();
    initialized_.Signal();
  }

  void RunOnNotifierThread(const base::Closure& callback) {
    base::WaitableEvent event(false, false);
    notifier_thread_proxy_->PostTask(FROM_HERE, base::Bind(
        &RunOnNotifierThreadHelper, callback, &event));
    event.Wait();
    // Run any tasks queued on the main thread, e.g. by
    // ObserverListThreadSafe.
    MessageLoop::current()->RunAllPending();
  }

  void SendResponse(uint32 state) {
    scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
    dbus::MessageWriter writer(response.get());
    writer.AppendVariantOfUint32(state);
    RunOnNotifierThread(base::Bind(response_callback_, response.get()));
  }

  void SendSignal(uint32 state) {
    dbus::Signal signal("org.freedesktop.NetworkManager", "StateChanged");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(state);
    RunOnNotifierThread(base::Bind(signal_callback_, &signal));
  }

  dbus::ObjectProxy::ResponseCallback response_callback_;
  dbus::ObjectProxy::SignalCallback signal_callback_;

  // Allows creating a new NetworkChangeNotifier.  Must be created before
  // |notifier_| and destroyed after it to avoid DCHECK failures.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  scoped_ptr<NetworkChangeNotifier> notifier_;

 private:
  static void RunOnNotifierThreadHelper(const base::Closure& callback,
                                        base::WaitableEvent* event) {
    callback.Run();
    event->Signal();
  }

  base::WaitableEvent initialized_;

  // Valid only after initialized_ is signaled.
  scoped_refptr<base::MessageLoopProxy> notifier_thread_proxy_;

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_object_proxy_;
};

namespace {

class OfflineObserver : public NetworkChangeNotifier::OnlineStateObserver {
 public:
  OfflineObserver()
      : notification_count(0),
        last_online_value(true) {
    NetworkChangeNotifier::AddOnlineStateObserver(this);
  }

  ~OfflineObserver() {
    NetworkChangeNotifier::RemoveOnlineStateObserver(this);
  }

  virtual void OnOnlineStateChanged(bool online) OVERRIDE {
    notification_count++;
    last_online_value = online;
  }

  int notification_count;
  bool last_online_value;
};

TEST_F(NetworkChangeNotifierLinuxTest, Offline) {
  SendResponse(NM_STATE_DISCONNECTED);
  EXPECT_TRUE(NetworkChangeNotifier::IsOffline());
}

TEST_F(NetworkChangeNotifierLinuxTest, Online) {
  SendResponse(NM_STATE_CONNECTED_GLOBAL);
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
}

TEST_F(NetworkChangeNotifierLinuxTest, OfflineThenOnline) {
  OfflineObserver observer;

  SendResponse(NM_STATE_DISCONNECTED);
  EXPECT_TRUE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(0, observer.notification_count);

  SendSignal(NM_STATE_CONNECTED_GLOBAL);
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(1, observer.notification_count);
  EXPECT_TRUE(observer.last_online_value);
}

TEST_F(NetworkChangeNotifierLinuxTest, MultipleStateChanges) {
  OfflineObserver observer;

  SendResponse(NM_STATE_CONNECTED_GLOBAL);
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(0, observer.notification_count);

  SendSignal(NM_STATE_DISCONNECTED);
  EXPECT_TRUE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(1, observer.notification_count);
  EXPECT_FALSE(observer.last_online_value);

  SendSignal(NM_STATE_CONNECTED_GLOBAL);
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(2, observer.notification_count);
  EXPECT_TRUE(observer.last_online_value);
}

TEST_F(NetworkChangeNotifierLinuxTest, IgnoreContinuedOnlineState) {
  OfflineObserver observer;

  SendResponse(NM_STATE_CONNECTED_SITE);
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(0, observer.notification_count);

  SendSignal(NM_STATE_CONNECTED_GLOBAL);
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(0, observer.notification_count);
}

TEST_F(NetworkChangeNotifierLinuxTest, IgnoreContinuedOfflineState) {
  OfflineObserver observer;

  SendResponse(NM_STATE_DISCONNECTING);
  EXPECT_TRUE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(0, observer.notification_count);

  SendSignal(NM_STATE_DISCONNECTED);
  EXPECT_TRUE(NetworkChangeNotifier::IsOffline());
  EXPECT_EQ(0, observer.notification_count);
}

TEST_F(NetworkChangeNotifierLinuxTest, NullResponse) {
  RunOnNotifierThread(base::Bind(
      response_callback_, static_cast<dbus::Response*>(NULL)));
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
}

TEST_F(NetworkChangeNotifierLinuxTest, EmptyResponse) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  RunOnNotifierThread(base::Bind(response_callback_, response.get()));
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
}

TEST_F(NetworkChangeNotifierLinuxTest, InvalidResponse) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendUint16(20);  // Uint16 instead of the expected Uint32
  RunOnNotifierThread(base::Bind(response_callback_, response.get()));
  EXPECT_FALSE(NetworkChangeNotifier::IsOffline());
}

}  // namespace
}  // namespace net
