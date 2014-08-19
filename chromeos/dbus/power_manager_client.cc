// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_manager_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/power_manager/input_event.pb.h"
#include "chromeos/dbus/power_manager/peripheral_battery_status.pb.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// Maximum amount of time that the power manager will wait for Chrome to
// say that it's ready for the system to be suspended, in milliseconds.
const int kSuspendDelayTimeoutMs = 5000;

// Human-readable description of Chrome's suspend delay.
const char kSuspendDelayDescription[] = "chrome";

// The PowerManagerClient implementation used in production.
class PowerManagerClientImpl : public PowerManagerClient {
 public:
  PowerManagerClientImpl()
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        power_manager_proxy_(NULL),
        suspend_delay_id_(-1),
        has_suspend_delay_id_(false),
        dark_suspend_delay_id_(-1),
        has_dark_suspend_delay_id_(false),
        pending_suspend_id_(-1),
        suspend_is_pending_(false),
        suspending_from_dark_resume_(false),
        num_pending_suspend_readiness_callbacks_(0),
        last_is_projecting_(false),
        weak_ptr_factory_(this) {}

  virtual ~PowerManagerClientImpl() {
    // Here we should unregister suspend notifications from powerd,
    // however:
    // - The lifetime of the PowerManagerClientImpl can extend past that of
    //   the objectproxy,
    // - power_manager can already detect that the client is gone and
    //   unregister our suspend delay.
  }

  // PowerManagerClient overrides:

  virtual void AddObserver(Observer* observer) OVERRIDE {
    CHECK(observer);  // http://crbug.com/119976
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kDecreaseScreenBrightnessMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(allow_off);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void IncreaseScreenBrightness() OVERRIDE {
    SimpleMethodCallToPowerManager(
        power_manager::kIncreaseScreenBrightnessMethod);
  }

  virtual void DecreaseKeyboardBrightness() OVERRIDE {
    SimpleMethodCallToPowerManager(
        power_manager::kDecreaseKeyboardBrightnessMethod);
  }

  virtual void IncreaseKeyboardBrightness() OVERRIDE {
    SimpleMethodCallToPowerManager(
        power_manager::kIncreaseKeyboardBrightnessMethod);
  }

  virtual void SetScreenBrightnessPercent(double percent,
                                          bool gradual) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kSetScreenBrightnessPercentMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendDouble(percent);
    writer.AppendInt32(
        gradual ?
        power_manager::kBrightnessTransitionGradual :
        power_manager::kBrightnessTransitionInstant);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void GetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kGetScreenBrightnessPercentMethod);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetScreenBrightnessPercent,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void RequestStatusUpdate() OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kGetPowerSupplyPropertiesMethod);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetPowerSupplyPropertiesMethod,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void RequestRestart() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kRequestRestartMethod);
  };

  virtual void RequestShutdown() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kRequestShutdownMethod);
  }

  virtual void NotifyUserActivity(
      power_manager::UserActivityType type) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kHandleUserActivityMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(type);

    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void NotifyVideoActivity(bool is_fullscreen) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kHandleVideoActivityMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(is_fullscreen);

    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetPolicy(
      const power_manager::PowerManagementPolicy& policy) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kSetPolicyMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(policy)) {
      LOG(ERROR) << "Error calling " << power_manager::kSetPolicyMethod;
      return;
    }
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void SetIsProjecting(bool is_projecting) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kSetIsProjectingMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(is_projecting);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
    last_is_projecting_ = is_projecting;
  }

  virtual base::Closure GetSuspendReadinessCallback() OVERRIDE {
    DCHECK(OnOriginThread());
    DCHECK(suspend_is_pending_);
    num_pending_suspend_readiness_callbacks_++;
    return base::Bind(&PowerManagerClientImpl::HandleObserverSuspendReadiness,
                      weak_ptr_factory_.GetWeakPtr(), pending_suspend_id_,
                      suspending_from_dark_resume_);
  }

  virtual int GetNumPendingSuspendReadinessCallbacks() OVERRIDE {
    return num_pending_suspend_readiness_callbacks_;
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    power_manager_proxy_ = bus->GetObjectProxy(
        power_manager::kPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kPowerManagerServicePath));

    power_manager_proxy_->SetNameOwnerChangedCallback(
        base::Bind(&PowerManagerClientImpl::NameOwnerChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for brightness changes. Only the power
    // manager knows the actual brightness level. We don't cache the
    // brightness level in Chrome as it'll make things less reliable.
    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kBrightnessChangedSignal,
        base::Bind(&PowerManagerClientImpl::BrightnessChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kPeripheralBatteryStatusSignal,
        base::Bind(&PowerManagerClientImpl::PeripheralBatteryStatusReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kPowerSupplyPollSignal,
        base::Bind(&PowerManagerClientImpl::PowerSupplyPollReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kInputEventSignal,
        base::Bind(&PowerManagerClientImpl::InputEventReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kSuspendImminentSignal,
        base::Bind(
            &PowerManagerClientImpl::HandleSuspendImminent,
            weak_ptr_factory_.GetWeakPtr(), false),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kSuspendDoneSignal,
        base::Bind(&PowerManagerClientImpl::SuspendDoneReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kDarkSuspendImminentSignal,
        base::Bind(
            &PowerManagerClientImpl::HandleSuspendImminent,
            weak_ptr_factory_.GetWeakPtr(), true),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kIdleActionImminentSignal,
        base::Bind(
            &PowerManagerClientImpl::IdleActionImminentReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kIdleActionDeferredSignal,
        base::Bind(
            &PowerManagerClientImpl::IdleActionDeferredReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    RegisterSuspendDelays();
  }

 private:
  // Returns true if the current thread is the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called when a dbus signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to signal "
                              << signal_name << ".";
  }

  // Makes a method call to power manager with no arguments and no response.
  void SimpleMethodCallToPowerManager(const std::string& method_name) {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 method_name);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    VLOG(1) << "Power manager restarted (old owner was "
            << (old_owner.empty() ? "[none]" : old_owner.c_str())
            << ", new owner is "
            << (new_owner.empty() ? "[none]" : new_owner.c_str()) << ")";
    if (!new_owner.empty()) {
      VLOG(1) << "Sending initial state to power manager";
      RegisterSuspendDelays();
      SetIsProjecting(last_is_projecting_);
      FOR_EACH_OBSERVER(Observer, observers_, PowerManagerRestarted());
    }
  }

  void BrightnessChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32_t brightness_level = 0;
    bool user_initiated = 0;
    if (!(reader.PopInt32(&brightness_level) &&
          reader.PopBool(&user_initiated))) {
      LOG(ERROR) << "Brightness changed signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    VLOG(1) << "Brightness changed to " << brightness_level
            << ": user initiated " << user_initiated;
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrightnessChanged(brightness_level, user_initiated));
  }

  void PeripheralBatteryStatusReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::PeripheralBatteryStatus protobuf_status;
    if (!reader.PopArrayOfBytesAsProto(&protobuf_status)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kPeripheralBatteryStatusSignal << " signal";
      return;
    }

    std::string path = protobuf_status.path();
    std::string name = protobuf_status.name();
    int level = protobuf_status.has_level() ? protobuf_status.level() : -1;

    VLOG(1) << "Device battery status received " << level
            << " for " << name << " at " << path;

    FOR_EACH_OBSERVER(Observer, observers_,
                      PeripheralBatteryStatusReceived(path, name, level));
  }

  void PowerSupplyPollReceived(dbus::Signal* signal) {
    VLOG(1) << "Received power supply poll signal.";
    dbus::MessageReader reader(signal);
    power_manager::PowerSupplyProperties protobuf;
    if (reader.PopArrayOfBytesAsProto(&protobuf)) {
      FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(protobuf));
    } else {
      LOG(ERROR) << "Unable to decode "
                 << power_manager::kPowerSupplyPollSignal << "signal";
    }
  }

  void OnGetPowerSupplyPropertiesMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kGetPowerSupplyPropertiesMethod;
      return;
    }

    dbus::MessageReader reader(response);
    power_manager::PowerSupplyProperties protobuf;
    if (reader.PopArrayOfBytesAsProto(&protobuf)) {
      FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(protobuf));
    } else {
      LOG(ERROR) << "Unable to decode "
                 << power_manager::kGetPowerSupplyPropertiesMethod
                 << " response";
    }
  }

  void OnGetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kGetScreenBrightnessPercentMethod;
      return;
    }
    dbus::MessageReader reader(response);
    double percent = 0.0;
    if (!reader.PopDouble(&percent))
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
    callback.Run(percent);
  }

  void HandleRegisterSuspendDelayReply(bool dark_suspend,
                                       dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << response->GetMember();
      return;
    }

    dbus::MessageReader reader(response);
    power_manager::RegisterSuspendDelayReply protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      LOG(ERROR) << "Unable to parse reply from " << response->GetMember();
      return;
    }

    if (dark_suspend) {
      dark_suspend_delay_id_ = protobuf.delay_id();
      has_dark_suspend_delay_id_ = true;
      VLOG(1) << "Registered dark suspend delay " << dark_suspend_delay_id_;
    } else {
      suspend_delay_id_ = protobuf.delay_id();
      has_suspend_delay_id_ = true;
      VLOG(1) << "Registered suspend delay " << suspend_delay_id_;
    }
  }

  void HandleSuspendImminent(bool in_dark_resume, dbus::Signal* signal) {
    std::string signal_name = signal->GetMember();
    if ((in_dark_resume && !has_dark_suspend_delay_id_) ||
        (!in_dark_resume && !has_suspend_delay_id_)) {
      LOG(ERROR) << "Received unrequested " << signal_name << " signal";
      return;
    }

    dbus::MessageReader reader(signal);
    power_manager::SuspendImminent proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Unable to decode protocol buffer from " << signal_name
                 << " signal";
      return;
    }

    VLOG(1) << "Got " << signal_name << " signal announcing suspend attempt "
            << proto.suspend_id();

    // If a previous suspend is pending from the same state we are currently in
    // (fully powered on or in dark resume), then something's gone a little
    // wonky.
    if (suspend_is_pending_ &&
        suspending_from_dark_resume_ == in_dark_resume) {
      LOG(WARNING) << "Got " << signal_name << " signal about pending suspend "
                   << "attempt " << proto.suspend_id() << " while still "
                   << "waiting on attempt " << pending_suspend_id_;
    }

    pending_suspend_id_ = proto.suspend_id();
    suspend_is_pending_ = true;
    suspending_from_dark_resume_ = in_dark_resume;
    num_pending_suspend_readiness_callbacks_ = 0;
    if (suspending_from_dark_resume_)
      FOR_EACH_OBSERVER(Observer, observers_, DarkSuspendImminent());
    else
      FOR_EACH_OBSERVER(Observer, observers_, SuspendImminent());
    MaybeReportSuspendReadiness();
  }

  void SuspendDoneReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::SuspendDone proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kSuspendDoneSignal << " signal";
      return;
    }

    const base::TimeDelta duration =
        base::TimeDelta::FromInternalValue(proto.suspend_duration());
    VLOG(1) << "Got " << power_manager::kSuspendDoneSignal << " signal:"
            << " suspend_id=" << proto.suspend_id()
            << " duration=" << duration.InSeconds() << " sec";
    FOR_EACH_OBSERVER(
        PowerManagerClient::Observer, observers_, SuspendDone(duration));
  }

  void IdleActionImminentReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::IdleActionImminent proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kIdleActionImminentSignal << " signal";
      return;
    }
    FOR_EACH_OBSERVER(Observer, observers_,
        IdleActionImminent(base::TimeDelta::FromInternalValue(
            proto.time_until_idle_action())));
  }

  void IdleActionDeferredReceived(dbus::Signal* signal) {
    FOR_EACH_OBSERVER(Observer, observers_, IdleActionDeferred());
  }

  void InputEventReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::InputEvent proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kInputEventSignal << " signal";
      return;
    }

    base::TimeTicks timestamp =
        base::TimeTicks::FromInternalValue(proto.timestamp());
    VLOG(1) << "Got " << power_manager::kInputEventSignal << " signal:"
            << " type=" << proto.type() << " timestamp=" << proto.timestamp();
    switch (proto.type()) {
      case power_manager::InputEvent_Type_POWER_BUTTON_DOWN:
      case power_manager::InputEvent_Type_POWER_BUTTON_UP: {
        const bool down =
            (proto.type() == power_manager::InputEvent_Type_POWER_BUTTON_DOWN);
        FOR_EACH_OBSERVER(PowerManagerClient::Observer, observers_,
                          PowerButtonEventReceived(down, timestamp));

        // Tell powerd that Chrome has handled power button presses.
        if (down) {
          dbus::MethodCall method_call(
              power_manager::kPowerManagerInterface,
              power_manager::kHandlePowerButtonAcknowledgmentMethod);
          dbus::MessageWriter writer(&method_call);
          writer.AppendInt64(proto.timestamp());
          power_manager_proxy_->CallMethod(
              &method_call,
              dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
              dbus::ObjectProxy::EmptyResponseCallback());
        }
        break;
      }
      case power_manager::InputEvent_Type_LID_OPEN:
      case power_manager::InputEvent_Type_LID_CLOSED: {
        bool open =
            (proto.type() == power_manager::InputEvent_Type_LID_OPEN);
        FOR_EACH_OBSERVER(PowerManagerClient::Observer, observers_,
                          LidEventReceived(open, timestamp));
        break;
      }
    }
  }

  void RegisterSuspendDelayImpl(
      const std::string& method_name,
      const power_manager::RegisterSuspendDelayRequest& protobuf_request,
      dbus::ObjectProxy::ResponseCallback callback) {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
      LOG(ERROR) << "Error constructing message for " << method_name;
      return;
    }

    power_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, callback);
  }

  // Registers suspend delays with the power manager.  This is usually only
  // called at startup, but if the power manager restarts, we need to create new
  // delays.
  void RegisterSuspendDelays() {
    // Throw out any old delay that was registered.
    suspend_delay_id_ = -1;
    has_suspend_delay_id_ = false;
    dark_suspend_delay_id_ = -1;
    has_dark_suspend_delay_id_ = false;

    power_manager::RegisterSuspendDelayRequest protobuf_request;
    base::TimeDelta timeout =
        base::TimeDelta::FromMilliseconds(kSuspendDelayTimeoutMs);
    protobuf_request.set_timeout(timeout.ToInternalValue());
    protobuf_request.set_description(kSuspendDelayDescription);

    RegisterSuspendDelayImpl(
        power_manager::kRegisterSuspendDelayMethod,
        protobuf_request,
        base::Bind(
            &PowerManagerClientImpl::HandleRegisterSuspendDelayReply,
            weak_ptr_factory_.GetWeakPtr(), false));
    RegisterSuspendDelayImpl(
        power_manager::kRegisterDarkSuspendDelayMethod,
        protobuf_request,
        base::Bind(
            &PowerManagerClientImpl::HandleRegisterSuspendDelayReply,
            weak_ptr_factory_.GetWeakPtr(), true));
  }

  // Records the fact that an observer has finished doing asynchronous work
  // that was blocking a pending suspend attempt and possibly reports
  // suspend readiness to powerd.  Called by callbacks returned via
  // GetSuspendReadinessCallback().
  void HandleObserverSuspendReadiness(int32_t suspend_id, bool in_dark_resume) {
    DCHECK(OnOriginThread());
    if (!suspend_is_pending_ || suspend_id != pending_suspend_id_ ||
        in_dark_resume != suspending_from_dark_resume_)
      return;

    num_pending_suspend_readiness_callbacks_--;
    MaybeReportSuspendReadiness();
  }

  // Reports suspend readiness to powerd if no observers are still holding
  // suspend readiness callbacks.
  void MaybeReportSuspendReadiness() {
    if (!suspend_is_pending_ || num_pending_suspend_readiness_callbacks_ > 0)
      return;

    std::string method_name;
    int32_t delay_id = -1;
    if (suspending_from_dark_resume_) {
      method_name = power_manager::kHandleDarkSuspendReadinessMethod;
      delay_id = dark_suspend_delay_id_;
    } else {
      method_name = power_manager::kHandleSuspendReadinessMethod;
      delay_id = suspend_delay_id_;
    }

    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    VLOG(1) << "Announcing readiness of suspend delay " << delay_id
            << " for suspend attempt " << pending_suspend_id_;
    power_manager::SuspendReadinessInfo protobuf_request;
    protobuf_request.set_delay_id(delay_id);
    protobuf_request.set_suspend_id(pending_suspend_id_);

    pending_suspend_id_ = -1;
    suspend_is_pending_ = false;

    if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
      LOG(ERROR) << "Error constructing message for " << method_name;
      return;
    }
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  dbus::ObjectProxy* power_manager_proxy_;
  ObserverList<Observer> observers_;

  // The delay_id_ obtained from the RegisterSuspendDelay request.
  int32_t suspend_delay_id_;
  bool has_suspend_delay_id_;

  // The delay_id_ obtained from the RegisterDarkSuspendDelay request.
  int32_t dark_suspend_delay_id_;
  bool has_dark_suspend_delay_id_;

  // powerd-supplied ID corresponding to an imminent suspend attempt that is
  // currently being delayed.
  int32_t pending_suspend_id_;
  bool suspend_is_pending_;

  // Set to true when the suspend currently being delayed was triggered during a
  // dark resume.  Since |pending_suspend_id_| and |suspend_is_pending_| are
  // both shared by normal and dark suspends, |suspending_from_dark_resume_|
  // helps distinguish the context within which these variables are being used.
  bool suspending_from_dark_resume_;

  // Number of callbacks that have been returned by
  // GetSuspendReadinessCallback() during the currently-pending suspend
  // attempt but have not yet been called.
  int num_pending_suspend_readiness_callbacks_;

  // Last state passed to SetIsProjecting().
  bool last_is_projecting_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PowerManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientImpl);
};

// The PowerManagerClient implementation used on Linux desktop,
// which does nothing.
class PowerManagerClientStubImpl : public PowerManagerClient {
 public:
  PowerManagerClientStubImpl()
      : discharging_(true),
        battery_percentage_(40),
        brightness_(50.0),
        pause_count_(2),
        cycle_count_(0),
        num_pending_suspend_readiness_callbacks_(0),
        weak_ptr_factory_(this) {}

  virtual ~PowerManagerClientStubImpl() {}

  int num_pending_suspend_readiness_callbacks() const {
    return num_pending_suspend_readiness_callbacks_;
  }

  // PowerManagerClient overrides:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    ParseCommandLineSwitch();
    if (power_cycle_delay_ != base::TimeDelta()) {
      update_timer_.Start(FROM_HERE,
                          power_cycle_delay_,
                          this,
                          &PowerManagerClientStubImpl::UpdateStatus);
    }
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE {
    VLOG(1) << "Requested to descrease screen brightness";
    SetBrightness(brightness_ - 5.0, true);
  }

  virtual void IncreaseScreenBrightness() OVERRIDE {
    VLOG(1) << "Requested to increase screen brightness";
    SetBrightness(brightness_ + 5.0, true);
  }

  virtual void SetScreenBrightnessPercent(double percent,
                                          bool gradual) OVERRIDE {
    VLOG(1) << "Requested to set screen brightness to " << percent << "% "
            << (gradual ? "gradually" : "instantaneously");
    SetBrightness(percent, false);
  }

  virtual void GetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback) OVERRIDE {
    callback.Run(brightness_);
  }

  virtual void DecreaseKeyboardBrightness() OVERRIDE {
    VLOG(1) << "Requested to descrease keyboard brightness";
  }

  virtual void IncreaseKeyboardBrightness() OVERRIDE {
    VLOG(1) << "Requested to increase keyboard brightness";
  }

  virtual void RequestStatusUpdate() OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&PowerManagerClientStubImpl::UpdateStatus,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void RequestRestart() OVERRIDE {}
  virtual void RequestShutdown() OVERRIDE {}

  virtual void NotifyUserActivity(
      power_manager::UserActivityType type) OVERRIDE {}
  virtual void NotifyVideoActivity(bool is_fullscreen) OVERRIDE {}
  virtual void SetPolicy(
      const power_manager::PowerManagementPolicy& policy) OVERRIDE {}
  virtual void SetIsProjecting(bool is_projecting) OVERRIDE {}
  virtual base::Closure GetSuspendReadinessCallback() OVERRIDE {
    num_pending_suspend_readiness_callbacks_++;
    return base::Bind(&PowerManagerClientStubImpl::HandleSuspendReadiness,
                      weak_ptr_factory_.GetWeakPtr());
  }
  virtual int GetNumPendingSuspendReadinessCallbacks() OVERRIDE {
    return num_pending_suspend_readiness_callbacks_;
  }

 private:
  void HandleSuspendReadiness() {
    num_pending_suspend_readiness_callbacks_--;
  }

  void UpdateStatus() {
    if (pause_count_ > 0) {
      pause_count_--;
      if (pause_count_ == 2)
        discharging_ = !discharging_;
    } else {
      if (discharging_)
        battery_percentage_ -= (battery_percentage_ <= 10 ? 1 : 10);
      else
        battery_percentage_ += (battery_percentage_ >= 10 ? 10 : 1);
      battery_percentage_ = std::min(std::max(battery_percentage_, 0), 100);
      // We pause at 0 and 100% so that it's easier to check those conditions.
      if (battery_percentage_ == 0 || battery_percentage_ == 100) {
        pause_count_ = 4;
        if (battery_percentage_ == 100)
          cycle_count_ = (cycle_count_ + 1) % 3;
      }
    }

    const int kSecondsToEmptyFullBattery = 3 * 60 * 60;  // 3 hours.
    int64 remaining_battery_time =
        std::max(1, battery_percentage_ * kSecondsToEmptyFullBattery / 100);

    props_.Clear();

    switch (cycle_count_) {
      case 0:
        // Say that the system is charging with AC connected and
        // discharging without any charger connected.
        props_.set_external_power(discharging_ ?
            power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED :
            power_manager::PowerSupplyProperties_ExternalPower_AC);
        break;
      case 1:
        // Say that the system is both charging and discharging on USB
        // (i.e. a low-power charger).
        props_.set_external_power(
            power_manager::PowerSupplyProperties_ExternalPower_USB);
        break;
      case 2:
        // Say that the system is both charging and discharging on AC.
        props_.set_external_power(
            power_manager::PowerSupplyProperties_ExternalPower_AC);
        break;
      default:
        NOTREACHED() << "Unhandled cycle " << cycle_count_;
    }

    if (battery_percentage_ == 100 && !discharging_) {
      props_.set_battery_state(
          power_manager::PowerSupplyProperties_BatteryState_FULL);
    } else if (!discharging_) {
      props_.set_battery_state(
          power_manager::PowerSupplyProperties_BatteryState_CHARGING);
      props_.set_battery_time_to_full_sec(std::max(static_cast<int64>(1),
          kSecondsToEmptyFullBattery - remaining_battery_time));
    } else {
      props_.set_battery_state(
          power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
      props_.set_battery_time_to_empty_sec(remaining_battery_time);
    }

    props_.set_battery_percent(battery_percentage_);
    props_.set_is_calculating_battery_time(pause_count_ > 1);

    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(props_));
  }

  void SetBrightness(double percent, bool user_initiated) {
    brightness_ = std::min(std::max(0.0, percent), 100.0);
    int brightness_level = static_cast<int>(brightness_);
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrightnessChanged(brightness_level, user_initiated));
  }

  void ParseCommandLineSwitch() {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (!command_line || !command_line->HasSwitch(switches::kPowerStub))
      return;
    std::string option_str =
        command_line->GetSwitchValueASCII(switches::kPowerStub);
    base::StringPairs string_pairs;
    base::SplitStringIntoKeyValuePairs(option_str, '=', ',', &string_pairs);
    for (base::StringPairs::iterator iter = string_pairs.begin();
         iter != string_pairs.end(); ++iter) {
      ParseOption((*iter).first, (*iter).second);
    }
  }

  void ParseOption(const std::string& arg0, const std::string& arg1) {
    if (arg0 == "cycle" || arg0 == "interactive") {
      int seconds = 1;
      if (!arg1.empty())
        base::StringToInt(arg1, &seconds);
      power_cycle_delay_ = base::TimeDelta::FromSeconds(seconds);
    }
  }

  base::TimeDelta power_cycle_delay_;  // Time over which to cycle power state
  bool discharging_;
  int battery_percentage_;
  double brightness_;
  int pause_count_;
  int cycle_count_;
  ObserverList<Observer> observers_;
  base::RepeatingTimer<PowerManagerClientStubImpl> update_timer_;
  power_manager::PowerSupplyProperties props_;

  // Number of callbacks returned by GetSuspendReadinessCallback() but not yet
  // invoked.
  int num_pending_suspend_readiness_callbacks_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PowerManagerClientStubImpl> weak_ptr_factory_;
};

PowerManagerClient::PowerManagerClient() {
}

PowerManagerClient::~PowerManagerClient() {
}

// static
PowerManagerClient* PowerManagerClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new PowerManagerClientImpl();
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new PowerManagerClientStubImpl();
}

}  // namespace chromeos
