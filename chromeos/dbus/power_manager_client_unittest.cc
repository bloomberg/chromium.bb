// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_manager_client.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace chromeos {

namespace {

// Shorthand for a few commonly-used constants.
const char* kInterface = power_manager::kPowerManagerInterface;
const char* kSuspendImminent = power_manager::kSuspendImminentSignal;
const char* kHandleSuspendReadiness =
    power_manager::kHandleSuspendReadinessMethod;

// Matcher that verifies that a dbus::Message has member |name|.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

// Matcher that verifies that a dbus::MethodCall has member |method_name| and
// contains a SuspendReadinessInfo protobuf referring to |suspend_id| and
// |delay_id|.
MATCHER_P3(IsSuspendReadiness, method_name, suspend_id, delay_id, "") {
  if (arg->GetMember() != method_name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  power_manager::SuspendReadinessInfo proto;
  if (!dbus::MessageReader(arg).PopArrayOfBytesAsProto(&proto)) {
    *result_listener << "does not contain SuspendReadinessInfo protobuf";
    return false;
  }
  if (proto.suspend_id() != suspend_id) {
    *result_listener << "suspend ID is " << proto.suspend_id();
    return false;
  }
  if (proto.delay_id() != delay_id) {
    *result_listener << "delay ID is " << proto.delay_id();
    return false;
  }
  return true;
}

// Runs |callback| with |response|. Needed due to ResponseCallback expecting a
// bare pointer rather than an std::unique_ptr.
void RunResponseCallback(dbus::ObjectProxy::ResponseCallback callback,
                         std::unique_ptr<dbus::Response> response) {
  callback.Run(response.get());
}

// Stub implementation of PowerManagerClient::Observer.
class TestObserver : public PowerManagerClient::Observer {
 public:
  explicit TestObserver(PowerManagerClient* client) : client_(client) {
    client_->AddObserver(this);
  }
  ~TestObserver() override { client_->RemoveObserver(this); }

  int num_suspend_imminent() const { return num_suspend_imminent_; }
  int num_suspend_done() const { return num_suspend_done_; }

  void set_take_suspend_readiness_callback(bool take_callback) {
    take_suspend_readiness_callback_ = take_callback;
  }

  // Runs |suspend_readiness_callback_|.
  bool RunSuspendReadinessCallback() WARN_UNUSED_RESULT {
    if (suspend_readiness_callback_.is_null())
      return false;

    auto cb = suspend_readiness_callback_;
    suspend_readiness_callback_.Reset();
    cb.Run();
    return true;
  }

  // PowerManagerClient::Observer:
  void SuspendImminent() override {
    num_suspend_imminent_++;
    if (take_suspend_readiness_callback_)
      suspend_readiness_callback_ = client_->GetSuspendReadinessCallback();
  }
  void SuspendDone(const base::TimeDelta& sleep_duration) override {
    num_suspend_done_++;
  }

 private:
  PowerManagerClient* client_;  // Not owned.

  // Number of times SuspendImminent() and SuspendDone() have been called.
  int num_suspend_imminent_ = 0;
  int num_suspend_done_ = 0;

  // Should SuspendImminent() call |client_|'s GetSuspendReadinessCallback()
  // method?
  bool take_suspend_readiness_callback_ = false;

  // Callback returned by |client_|'s GetSuspendReadinessCallback() method.
  base::Closure suspend_readiness_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

// Stub implementation of PowerManagerClient::RenderProcessManagerDelegate.
class TestDelegate : public PowerManagerClient::RenderProcessManagerDelegate {
 public:
  explicit TestDelegate(PowerManagerClient* client) : weak_ptr_factory_(this) {
    client->SetRenderProcessManagerDelegate(weak_ptr_factory_.GetWeakPtr());
  }
  ~TestDelegate() override {}

  int num_suspend_imminent() const { return num_suspend_imminent_; }
  int num_suspend_done() const { return num_suspend_done_; }

  // PowerManagerClient::RenderProcessManagerDelegate:
  void SuspendImminent() override { num_suspend_imminent_++; }
  void SuspendDone() override { num_suspend_done_++; }

 private:
  // Number of times SuspendImminent() and SuspendDone() have been called.
  int num_suspend_imminent_ = 0;
  int num_suspend_done_ = 0;

  base::WeakPtrFactory<TestDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class PowerManagerClientTest : public testing::Test {
 public:
  PowerManagerClientTest() {}
  ~PowerManagerClientTest() override {}

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    proxy_ = new dbus::MockObjectProxy(
        bus_.get(), power_manager::kPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kPowerManagerServicePath));

    // |client_|'s Init() method should request a proxy for communicating with
    // powerd.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(
                    power_manager::kPowerManagerServiceName,
                    dbus::ObjectPath(power_manager::kPowerManagerServicePath)))
        .WillRepeatedly(Return(proxy_.get()));

    // Save |client_|'s signal and name-owner-changed callbacks.
    EXPECT_CALL(*proxy_.get(), ConnectToSignal(kInterface, _, _, _))
        .WillRepeatedly(Invoke(this, &PowerManagerClientTest::ConnectToSignal));
    EXPECT_CALL(*proxy_.get(), SetNameOwnerChangedCallback(_))
        .WillRepeatedly(SaveArg<0>(&name_owner_changed_callback_));

    // |client_|'s Init() method should register regular and dark suspend
    // delays.
    EXPECT_CALL(
        *proxy_.get(),
        CallMethod(HasMember(power_manager::kRegisterSuspendDelayMethod), _, _))
        .WillRepeatedly(
            Invoke(this, &PowerManagerClientTest::RegisterSuspendDelay));
    EXPECT_CALL(
        *proxy_.get(),
        CallMethod(HasMember(power_manager::kRegisterDarkSuspendDelayMethod), _,
                   _))
        .WillRepeatedly(
            Invoke(this, &PowerManagerClientTest::RegisterSuspendDelay));

    client_.reset(PowerManagerClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION));
    client_->Init(bus_.get());

    // Execute callbacks posted by Init().
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { client_.reset(); }

 protected:
  // Synchronously passes |signal| to |client_|'s handler, simulating the signal
  // being emitted by powerd.
  void EmitSignal(dbus::Signal* signal) {
    const std::string signal_name = signal->GetMember();
    const auto it = signal_callbacks_.find(signal_name);
    ASSERT_TRUE(it != signal_callbacks_.end())
        << "Client didn't register for signal " << signal_name;
    it->second.Run(signal);
  }

  // Passes a SuspendImminent or DarkSuspendImminent signal to |client_|.
  void EmitSuspendImminentSignal(const std::string& signal_name,
                                 int suspend_id) {
    power_manager::SuspendImminent proto;
    proto.set_suspend_id(suspend_id);
    dbus::Signal signal(kInterface, signal_name);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
    EmitSignal(&signal);
  }

  // Passes a SuspendDone signal to |client_|.
  void EmitSuspendDoneSignal(int suspend_id) {
    power_manager::SuspendDone proto;
    proto.set_suspend_id(suspend_id);
    dbus::Signal signal(kInterface, power_manager::kSuspendDoneSignal);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
    EmitSignal(&signal);
  }

  // Adds an expectation to |proxy_| for a HandleSuspendReadiness or
  // HandleDarkSuspendReadiness method call.
  void ExpectSuspendReadiness(const std::string& method_name,
                              int suspend_id,
                              int delay_id) {
    EXPECT_CALL(
        *proxy_.get(),
        CallMethod(IsSuspendReadiness(method_name, suspend_id, delay_id), _,
                   _));
  }

  // Arbitrary delay IDs returned to |client_|.
  static const int kSuspendDelayId = 100;
  static const int kDarkSuspendDelayId = 200;

  base::MessageLoop message_loop_;

  // Mock bus and proxy for simulating calls to powerd.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;

  std::unique_ptr<PowerManagerClient> client_;

  // Maps from powerd signal name to the corresponding callback provided by
  // |client_|.
  std::map<std::string, dbus::ObjectProxy::SignalCallback> signal_callbacks_;

  // Callback passed to |proxy_|'s SetNameOwnerChangedCallback() method.
  // TODO(derat): Test that |client_| handles powerd restarts.
  dbus::ObjectProxy::NameOwnerChangedCallback name_owner_changed_callback_;

 private:
  // Handles calls to |proxy_|'s ConnectToSignal() method.
  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    CHECK_EQ(interface_name, power_manager::kPowerManagerInterface);
    signal_callbacks_[signal_name] = signal_callback;

    message_loop_.task_runner()->PostTask(
        FROM_HERE, base::Bind(on_connected_callback, interface_name,
                              signal_name, true /* success */));
  }

  // Handles calls to |proxy_|'s CallMethod() method to register suspend delays.
  void RegisterSuspendDelay(dbus::MethodCall* method_call,
                            int timeout_ms,
                            dbus::ObjectProxy::ResponseCallback callback) {
    power_manager::RegisterSuspendDelayReply proto;
    proto.set_delay_id(method_call->GetMember() ==
                               power_manager::kRegisterDarkSuspendDelayMethod
                           ? kDarkSuspendDelayId
                           : kSuspendDelayId);

    method_call->SetSerial(123);  // Arbitrary but needed by FromMethodCall().
    std::unique_ptr<dbus::Response> response(
        dbus::Response::FromMethodCall(method_call));
    CHECK(dbus::MessageWriter(response.get()).AppendProtoAsArrayOfBytes(proto));

    message_loop_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&RunResponseCallback, callback, base::Passed(&response)));
  }

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientTest);
};

// Suspend readiness should be reported immediately when there are no observers.
TEST_F(PowerManagerClientTest, ReportSuspendReadinessWithoutObservers) {
  const int kSuspendId = 1;
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EmitSuspendDoneSignal(kSuspendId);
}

// Observers should be notified when suspend is imminent and done. Readiness
// should be reported synchronously when GetSuspendReadinessCallback() hasn't
// been called.
TEST_F(PowerManagerClientTest, ReportSuspendReadinessWithoutCallbacks) {
  TestObserver observer_1(client_.get());
  TestObserver observer_2(client_.get());

  const int kSuspendId = 1;
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_EQ(1, observer_1.num_suspend_imminent());
  EXPECT_EQ(0, observer_1.num_suspend_done());
  EXPECT_EQ(1, observer_2.num_suspend_imminent());
  EXPECT_EQ(0, observer_2.num_suspend_done());

  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, observer_1.num_suspend_imminent());
  EXPECT_EQ(1, observer_1.num_suspend_done());
  EXPECT_EQ(1, observer_2.num_suspend_imminent());
  EXPECT_EQ(1, observer_2.num_suspend_done());
}

// When observers call GetSuspendReadinessCallback() from their
// SuspendImminent() methods, the HandleSuspendReadiness method call should be
// deferred until all callbacks are run.
TEST_F(PowerManagerClientTest, ReportSuspendReadinessWithCallbacks) {
  TestObserver observer_1(client_.get());
  observer_1.set_take_suspend_readiness_callback(true);
  TestObserver observer_2(client_.get());
  observer_2.set_take_suspend_readiness_callback(true);
  TestObserver observer_3(client_.get());

  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_TRUE(observer_1.RunSuspendReadinessCallback());
  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EXPECT_TRUE(observer_2.RunSuspendReadinessCallback());
  EmitSuspendDoneSignal(kSuspendId);
}

// The RenderProcessManagerDelegate should be notified that suspend is imminent
// only after observers have reported readiness.
TEST_F(PowerManagerClientTest, NotifyRenderProcessManagerDelegate) {
  TestDelegate delegate(client_.get());
  TestObserver observer(client_.get());
  observer.set_take_suspend_readiness_callback(true);

  const int kSuspendId = 1;
  EmitSuspendImminentSignal(kSuspendImminent, kSuspendId);
  EXPECT_EQ(0, delegate.num_suspend_imminent());
  EXPECT_EQ(0, delegate.num_suspend_done());

  ExpectSuspendReadiness(kHandleSuspendReadiness, kSuspendId, kSuspendDelayId);
  EXPECT_TRUE(observer.RunSuspendReadinessCallback());
  EXPECT_EQ(1, delegate.num_suspend_imminent());
  EXPECT_EQ(0, delegate.num_suspend_done());

  EmitSuspendDoneSignal(kSuspendId);
  EXPECT_EQ(1, delegate.num_suspend_imminent());
  EXPECT_EQ(1, delegate.num_suspend_done());
}

// TODO(derat): Add more tests, e.g. for SuspendDone being received while
// readiness callbacks are still outstanding (http://crbug.com/646912) and for
// the handling of DarkSuspendImminent signals.

}  // namespace chromeos
