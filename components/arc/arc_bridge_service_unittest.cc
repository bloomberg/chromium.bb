// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/common/arc_host_messages.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// A fake sender that can connect to a specified IPC::ChannelHandle.
class IPCSenderFake : public IPC::Listener, public IPC::Sender {
 public:
  explicit IPCSenderFake(
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
      : ipc_task_runner_(ipc_task_runner) {}
  ~IPCSenderFake() override {}

  // Connects as a client to the specified |handle|.
  bool Connect(const IPC::ChannelHandle& handle) {
    ipc_channel_ = IPC::ChannelProxy::Create(handle, IPC::Channel::MODE_CLIENT,
                                             this, ipc_task_runner_.get());
    return ipc_channel_;
  }

  bool Send(IPC::Message* msg) override { return ipc_channel_->Send(msg); }

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

 private:
  // Task runner on which ipc operations are performed.
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // The channel through which messages are sent.
  scoped_ptr<IPC::ChannelProxy> ipc_channel_;

  DISALLOW_COPY_AND_ASSIGN(IPCSenderFake);
};

}  // namespace

class ArcBridgeTest : public testing::Test, public ArcBridgeService::Observer {
 public:
  ArcBridgeTest() : ready_(false) {}
  ~ArcBridgeTest() override {}

  void OnStateChanged(ArcBridgeService::State state) override {
    state_ = state;
    switch (state) {
      case ArcBridgeService::State::READY:
        ready_ = true;
        break;

      case ArcBridgeService::State::STOPPED:
        message_loop_.PostTask(FROM_HERE, message_loop_.QuitWhenIdleClosure());
        break;

      default:
        break;
    }
  }

  void OnInstanceBootPhase(InstanceBootPhase boot_phase) override {
    boot_phase_ = boot_phase;
  }

  bool ready() const { return ready_; }
  InstanceBootPhase boot_phase() const { return boot_phase_; }
  ArcBridgeService::State state() const { return state_; }

 protected:
  scoped_ptr<IPCSenderFake> fake_sender_;

  scoped_ptr<ArcBridgeServiceImpl> service_;

 private:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    ready_ = false;
    boot_phase_ = InstanceBootPhase::NOT_RUNNING;

    ipc_thread_.reset(new base::Thread("IPC thread"));
    ipc_thread_->StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    service_.reset(new ArcBridgeServiceImpl(ipc_thread_->task_runner(),
                                            message_loop_.task_runner()));

    service_->AddObserver(this);

    IPC::ChannelHandle handle(IPC::Channel::GenerateUniqueRandomChannelID());
    // Testing code does not do all the steps that are done by regular
    // connection. In particular, it does not need to create a directory for
    // the socket, so manually set the state to CONNECTING.
    service_->SetState(ArcBridgeService::State::CONNECTING);
    // Connect directly to the specified channel instead of going through
    // D-Bus, since it is not available for tests.
    EXPECT_TRUE(service_->Connect(handle, IPC::Channel::MODE_SERVER));
    // Testing code does also not send D-Bus messages, so set the state to
    // STARTING.
    service_->SetState(ArcBridgeService::State::STARTING);
    fake_sender_.reset(new IPCSenderFake(ipc_thread_->task_runner()));
    EXPECT_TRUE(fake_sender_);
    EXPECT_TRUE(fake_sender_->Connect(handle));
  }

  void TearDown() override {
    fake_sender_.reset();
    service_->RemoveObserver(this);
    service_.reset();
    ipc_thread_.reset();

    chromeos::DBusThreadManager::Shutdown();
  }

  bool ready_;
  InstanceBootPhase boot_phase_;
  ArcBridgeService::State state_;
  base::MessageLoopForUI message_loop_;

  // Thread in which IPC messaging is performed.
  scoped_ptr<base::Thread> ipc_thread_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeTest);
};

// Shuts down the instance reports booted.
class ScopedShutdownWhenBooted : public ArcBridgeService::Observer {
 public:
  ScopedShutdownWhenBooted(ArcBridgeService* service) : service_(service) {
    service_->AddObserver(this);
  }

  ~ScopedShutdownWhenBooted() override { service_->RemoveObserver(this); }

  void OnInstanceBootPhase(InstanceBootPhase boot_phase) override {
    if (boot_phase == InstanceBootPhase::BOOT_COMPLETED) {
      service_->Shutdown();
    }
  }

 private:
  ArcBridgeService* service_;

  DISALLOW_COPY_AND_ASSIGN(ScopedShutdownWhenBooted);
};

// Exercises the basic functionality of the ARC Bridge Service.  A message from
// within the instance should cause the observer to be notified.
TEST_F(ArcBridgeTest, Basic) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(ArcBridgeService::State::STARTING, state());

  ScopedShutdownWhenBooted shutdown(service_.get());

  ASSERT_TRUE(fake_sender_->Send(new ArcInstanceHostMsg_InstanceBootPhase(
      InstanceBootPhase::BRIDGE_READY)));
  ASSERT_TRUE(fake_sender_->Send(new ArcInstanceHostMsg_InstanceBootPhase(
      InstanceBootPhase::BOOT_COMPLETED)));

  base::RunLoop run_loop;
  run_loop.Run();

  EXPECT_TRUE(ready());
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
  ASSERT_EQ(InstanceBootPhase::BOOT_COMPLETED, boot_phase());
}

// If the ArcBridgeService is shut down, it should be stopped, even
// mid-startup.
TEST_F(ArcBridgeTest, ShutdownMidStartup) {
  ASSERT_FALSE(ready());

  ASSERT_EQ(ArcBridgeService::State::STARTING, state());
  service_->Shutdown();
  // Some machines can reach the STOPPED state immediately.
  ASSERT_TRUE(state() == ArcBridgeService::State::STOPPING ||
              state() == ArcBridgeService::State::STOPPED);

  base::RunLoop run_loop;
  run_loop.Run();

  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

}  // namespace arc
