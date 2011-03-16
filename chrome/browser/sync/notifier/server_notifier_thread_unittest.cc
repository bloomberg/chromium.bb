// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/sync/notifier/server_notifier_thread.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

class FakeServerNotifierThread : public ServerNotifierThread {
 public:
   FakeServerNotifierThread()
      : ServerNotifierThread(notifier::NotifierOptions(), "fake state",
                             ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual ~FakeServerNotifierThread() {}

  virtual void Start() {
    DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
    ServerNotifierThread::Start();
    worker_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FakeServerNotifierThread::InitFakes));
  }

  virtual void Logout() {
    DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
    worker_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FakeServerNotifierThread::DestroyFakes));
    ServerNotifierThread::Logout();
  }

  // We prevent the real connection attempt from happening and use
  // SimulateConnection()/SimulateDisconnection() instead.
  virtual void Login(const buzz::XmppClientSettings& settings) {
    DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  }

  // We pass ourselves as the StateWriter in the constructor, so shim
  // out WriteState() to prevent an infinite loop.
  virtual void WriteState(const std::string& state) {
    DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  }

  void SimulateConnect() {
    DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
    worker_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FakeServerNotifierThread::DoSimulateConnect));
  }

  void SimulateDisconnect() {
    DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
    worker_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FakeServerNotifierThread::DoSimulateDisconnect));
  }

 private:
  void InitFakes() {
    DCHECK_EQ(MessageLoop::current(), worker_message_loop());
    fake_base_task_.reset(new notifier::FakeBaseTask());
  }

  void DestroyFakes() {
    DCHECK_EQ(MessageLoop::current(), worker_message_loop());
    fake_base_task_.reset();
  }

  void DoSimulateConnect() {
    DCHECK_EQ(MessageLoop::current(), worker_message_loop());
    OnConnect(fake_base_task_->AsWeakPtr());
  }

  void DoSimulateDisconnect() {
    DCHECK_EQ(MessageLoop::current(), worker_message_loop());
    OnDisconnect();
  }

  // Used only on the worker thread.
  scoped_ptr<notifier::FakeBaseTask> fake_base_task_;
};

}  // namespace sync_notifier

DISABLE_RUNNABLE_METHOD_REFCOUNT(sync_notifier::FakeServerNotifierThread);

namespace sync_notifier {

namespace {

class ServerNotifierThreadTest : public testing::Test {
 protected:
  MessageLoop message_loop_;
};

syncable::ModelTypeSet GetTypeSetWithAllTypes() {
  syncable::ModelTypeSet all_types;

  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    all_types.insert(model_type);
  }

  return all_types;
}

TEST_F(ServerNotifierThreadTest, Basic) {
  FakeServerNotifierThread server_notifier_thread;

  server_notifier_thread.Start();
  server_notifier_thread.Login(buzz::XmppClientSettings());
  server_notifier_thread.SimulateConnect();
  server_notifier_thread.ListenForUpdates();
  server_notifier_thread.SubscribeForUpdates(notifier::SubscriptionList());
  server_notifier_thread.Logout();
}

TEST_F(ServerNotifierThreadTest, DisconnectBeforeListen) {
  FakeServerNotifierThread server_notifier_thread;

  server_notifier_thread.Start();
  server_notifier_thread.Login(buzz::XmppClientSettings());
  server_notifier_thread.ListenForUpdates();
  server_notifier_thread.SubscribeForUpdates(notifier::SubscriptionList());
  server_notifier_thread.Logout();
}

TEST_F(ServerNotifierThreadTest, Disconnected) {
  FakeServerNotifierThread server_notifier_thread;

  server_notifier_thread.Start();
  server_notifier_thread.Login(buzz::XmppClientSettings());
  server_notifier_thread.SimulateConnect();
  server_notifier_thread.SimulateDisconnect();
  server_notifier_thread.ListenForUpdates();
  server_notifier_thread.SubscribeForUpdates(notifier::SubscriptionList());
  server_notifier_thread.Logout();
}

}  // namespace

}  // namespace sync_notifier
