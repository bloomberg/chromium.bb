// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/test/fake_arc_bridge_instance.h"
#include "ipc/mojo/scoped_ipc_support.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// A fake ArcBridgeBootstrap that creates a local connection.
class FakeArcBridgeBootstrap : public ArcBridgeBootstrap {
 public:
  explicit FakeArcBridgeBootstrap(FakeArcBridgeInstance* instance)
      : instance_(instance) {}
  ~FakeArcBridgeBootstrap() override {}

  void Start() override {
    DCHECK(delegate_);
    ArcBridgeInstancePtr instance;
    instance_->Bind(mojo::GetProxy(&instance));
    delegate_->OnConnectionEstablished(std::move(instance));
  }

  void Stop() override {
    DCHECK(delegate_);
    instance_->Unbind();
    delegate_->OnStopped();
  }

 private:
  // Owned by the caller.
  FakeArcBridgeInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeBootstrap);
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

  bool ready() const { return ready_; }
  ArcBridgeService::State state() const { return state_; }

 protected:
  scoped_ptr<ArcBridgeServiceImpl> service_;
  scoped_ptr<FakeArcBridgeInstance> instance_;

 private:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    ready_ = false;
    state_ = ArcBridgeService::State::STOPPED;

    instance_.reset(new FakeArcBridgeInstance());
    service_.reset(new ArcBridgeServiceImpl(
        make_scoped_ptr(new FakeArcBridgeBootstrap(instance_.get()))));

    service_->AddObserver(this);
  }

  void TearDown() override {
    service_->RemoveObserver(this);
    instance_.reset();
    service_.reset();

    chromeos::DBusThreadManager::Shutdown();
  }

  bool ready_;
  ArcBridgeService::State state_;
  base::MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeTest);
};

// Exercises the basic functionality of the ARC Bridge Service.  A message from
// within the instance should cause the observer to be notified.
TEST_F(ArcBridgeTest, Basic) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());

  service_->SetAvailable(true);
  service_->HandleStartup();
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If not all pre-requisites are met, the instance is not started.
TEST_F(ArcBridgeTest, Prerequisites) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
  service_->SetAvailable(true);
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
  service_->SetAvailable(false);
  service_->HandleStartup();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If the ArcBridgeService is shut down, it should be stopped, even
// mid-startup.
TEST_F(ArcBridgeTest, ShutdownMidStartup) {
  ASSERT_FALSE(ready());

  service_->SetAvailable(true);
  service_->HandleStartup();
  // WaitForInitCall() omitted.
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If the channel is disconnected, it should be re-established.
TEST_F(ArcBridgeTest, Restart) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(0, instance_->init_calls());

  service_->SetAvailable(true);
  service_->HandleStartup();
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::State::READY, state());
  ASSERT_EQ(1, instance_->init_calls());

  // Simulate a connection loss.
  service_->OnChannelClosed();
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::State::READY, state());
  ASSERT_EQ(2, instance_->init_calls());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

}  // namespace arc
