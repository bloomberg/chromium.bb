// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power_manager_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/timer.h"
#include "chromeos/dbus/power_manager/input_event.pb.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/dbus/power_state_control.pb.h"
#include "chromeos/dbus/power_supply_properties.pb.h"
#include "chromeos/dbus/video_activity_update.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Maximum amount of time that the power manager will wait for Chrome to
// say that it's ready for the system to be suspended, in milliseconds.
const int kSuspendDelayTimeoutMs = 5000;

// Human-readable description of Chrome's suspend delay.
const char kSuspendDelayDescription[] = "chrome";

// The PowerManagerClient implementation used in production.
class PowerManagerClientImpl : public PowerManagerClient {
 public:
  explicit PowerManagerClientImpl(dbus::Bus* bus)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        power_manager_proxy_(NULL),
        suspend_delay_id_(-1),
        has_suspend_delay_id_(false),
        pending_suspend_id_(-1),
        suspend_is_pending_(false),
        num_pending_suspend_readiness_callbacks_(0),
        weak_ptr_factory_(this) {
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

    // TODO(derat): Stop listening for this.
    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kSetScreenPowerSignal,
        base::Bind(&PowerManagerClientImpl::ScreenPowerSignalReceived,
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
        power_manager::kIdleNotifySignal,
        base::Bind(&PowerManagerClientImpl::IdleNotifySignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kSoftwareScreenDimmingRequestedSignal,
        base::Bind(
            &PowerManagerClientImpl::SoftwareScreenDimmingRequestedReceived,
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
        power_manager::kSuspendStateChangedSignal,
        base::Bind(&PowerManagerClientImpl::SuspendStateChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    power_manager_proxy_->ConnectToSignal(
        power_manager::kPowerManagerInterface,
        power_manager::kSuspendImminentSignal,
        base::Bind(
            &PowerManagerClientImpl::SuspendImminentReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&PowerManagerClientImpl::SignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));

    RegisterSuspendDelay();
  }

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
        power_manager::kDecreaseScreenBrightness);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(allow_off);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void IncreaseScreenBrightness() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kIncreaseScreenBrightness);
  }

  virtual void DecreaseKeyboardBrightness() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kDecreaseKeyboardBrightness);
  }

  virtual void IncreaseKeyboardBrightness() OVERRIDE {
    SimpleMethodCallToPowerManager(power_manager::kIncreaseKeyboardBrightness);
  }

  virtual void SetScreenBrightnessPercent(double percent,
                                          bool gradual) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kSetScreenBrightnessPercent);
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
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetScreenBrightnessPercent);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetScreenBrightnessPercent,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void RequestStatusUpdate(UpdateRequestType update_type) OVERRIDE {
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

  virtual void CalculateIdleTime(const CalculateIdleTimeCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kGetIdleTime);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnGetIdleTime,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void RequestIdleNotification(int64 threshold) OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kRequestIdleNotification);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt64(threshold);

    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void NotifyUserActivity(
      const base::TimeTicks& last_activity_time) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kHandleUserActivityMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt64(last_activity_time.ToInternalValue());
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  virtual void NotifyVideoActivity(
      const base::TimeTicks& last_activity_time,
      bool is_fullscreen) OVERRIDE {
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kHandleVideoActivityMethod);
    dbus::MessageWriter writer(&method_call);

    VideoActivityUpdate protobuf;
    protobuf.set_last_activity_time(last_activity_time.ToInternalValue());
    protobuf.set_is_fullscreen(is_fullscreen);

    if (!writer.AppendProtoAsArrayOfBytes(protobuf)) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kHandleVideoActivityMethod;
      return;
    }
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

  virtual void RequestPowerStateOverrides(
      uint32 request_id,
      base::TimeDelta duration,
      int overrides,
      const PowerStateRequestIdCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kStateOverrideRequest);
    dbus::MessageWriter writer(&method_call);

    PowerStateControl protobuf;
    protobuf.set_request_id(request_id);
    protobuf.set_duration(duration.InSeconds());
    protobuf.set_disable_idle_dim(overrides & DISABLE_IDLE_DIM);
    protobuf.set_disable_idle_blank(overrides & DISABLE_IDLE_BLANK);
    protobuf.set_disable_idle_suspend(overrides & DISABLE_IDLE_SUSPEND);
    protobuf.set_disable_lid_suspend(overrides & DISABLE_LID_SUSPEND);

    if (!writer.AppendProtoAsArrayOfBytes(protobuf)) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kStateOverrideRequest;
      return;
    }
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PowerManagerClientImpl::OnPowerStateOverride,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void CancelPowerStateOverrides(uint32 request_id) OVERRIDE {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kStateOverrideCancel);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(request_id);
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
  }

  virtual base::Closure GetSuspendReadinessCallback() OVERRIDE {
    DCHECK(OnOriginThread());
    DCHECK(suspend_is_pending_);
    num_pending_suspend_readiness_callbacks_++;
    return base::Bind(&PowerManagerClientImpl::HandleObserverSuspendReadiness,
                      weak_ptr_factory_.GetWeakPtr(), pending_suspend_id_);
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

  // Make a method call to power manager with no arguments and no response.
  void SimpleMethodCallToPowerManager(const std::string& method_name) {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 method_name);
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        dbus::ObjectProxy::EmptyResponseCallback());
  }

  void NameOwnerChangedReceived(dbus::Signal* signal) {
    VLOG(1) << "Power manager restarted";
    RegisterSuspendDelay();
    FOR_EACH_OBSERVER(Observer, observers_, PowerManagerRestarted());
  }

  void BrightnessChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32 brightness_level = 0;
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

  void ScreenPowerSignalReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool dbus_power_on = false;
    bool dbus_all_displays = false;
    if (reader.PopBool(&dbus_power_on) &&
        reader.PopBool(&dbus_all_displays)) {
      VLOG(1) << "Screen power set to " << dbus_power_on
              << " for all displays " << dbus_all_displays;
      FOR_EACH_OBSERVER(Observer, observers_,
                        ScreenPowerSet(dbus_power_on, dbus_all_displays));
    } else {
      LOG(ERROR) << "screen power signal had incorrect parameters: "
                 << signal->ToString();
    }
  }

  void PowerSupplyPollReceived(dbus::Signal* unused_signal) {
    VLOG(1) << "Received power supply poll signal.";
    RequestStatusUpdate(UPDATE_POLL);
  }

  void OnGetPowerSupplyPropertiesMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kGetPowerSupplyPropertiesMethod;
      return;
    }

    dbus::MessageReader reader(response);
    PowerSupplyProperties protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kGetPowerSupplyPropertiesMethod
                 << response->ToString();
      return;
    }

    PowerSupplyStatus status;
    status.line_power_on = protobuf.line_power_on();
    status.battery_seconds_to_empty = protobuf.battery_time_to_empty();
    status.battery_seconds_to_full = protobuf.battery_time_to_full();
    status.averaged_battery_time_to_empty =
        protobuf.averaged_battery_time_to_empty();
    status.averaged_battery_time_to_full =
        protobuf.averaged_battery_time_to_full();
    status.battery_percentage = protobuf.battery_percentage();
    status.battery_is_present = protobuf.battery_is_present();
    status.battery_is_full = protobuf.battery_is_charged();
    status.is_calculating_battery_time = protobuf.is_calculating_battery_time();
    status.battery_energy_rate = protobuf.battery_energy_rate();

    VLOG(1) << "Power status: " << status.ToString();
    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(status));
  }

  void OnGetIdleTime(const CalculateIdleTimeCallback& callback,
                     dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << power_manager::kGetIdleTime;
      return;
    }
    dbus::MessageReader reader(response);
    int64 idle_time_ms = 0;
    if (!reader.PopInt64(&idle_time_ms)) {
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
      callback.Run(-1);
      return;
    }
    if (idle_time_ms < 0) {
      LOG(ERROR) << "Power manager failed to calculate idle time.";
      callback.Run(-1);
      return;
    }
    callback.Run(idle_time_ms/1000);
  }

  void OnPowerStateOverride(const PowerStateRequestIdCallback& callback,
                            dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << power_manager::kStateOverrideRequest;
      return;
    }

    dbus::MessageReader reader(response);
    int32 request_id = 0;
    if (!reader.PopInt32(&request_id)) {
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
      callback.Run(0);
      return;
    }

    callback.Run(request_id);
  }

  void OnGetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kGetScreenBrightnessPercent;
      return;
    }
    dbus::MessageReader reader(response);
    double percent = 0.0;
    if (!reader.PopDouble(&percent))
      LOG(ERROR) << "Error reading response from powerd: "
                 << response->ToString();
    callback.Run(percent);
  }

  void OnRegisterSuspendDelayReply(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << power_manager::kRegisterSuspendDelayMethod;
      return;
    }

    dbus::MessageReader reader(response);
    power_manager::RegisterSuspendDelayReply protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      LOG(ERROR) << "Unable to parse reply from "
                 << power_manager::kRegisterSuspendDelayMethod;
      return;
    }

    suspend_delay_id_ = protobuf.delay_id();
    has_suspend_delay_id_ = true;
    VLOG(1) << "Registered suspend delay " << suspend_delay_id_;
  }

  void IdleNotifySignalReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int64 threshold = 0;
    if (!reader.PopInt64(&threshold)) {
      LOG(ERROR) << "Idle Notify signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    DCHECK_GT(threshold, 0);

    VLOG(1) << "Idle Notify: " << threshold;
    FOR_EACH_OBSERVER(Observer, observers_, IdleNotify(threshold));
  }

  void SoftwareScreenDimmingRequestedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32 signal_state = 0;
    if (!reader.PopInt32(&signal_state)) {
      LOG(ERROR) << "Screen dimming signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }

    Observer::ScreenDimmingState state = Observer::SCREEN_DIMMING_NONE;
    switch (signal_state) {
      case power_manager::kSoftwareScreenDimmingNone:
        state = Observer::SCREEN_DIMMING_NONE;
        break;
      case power_manager::kSoftwareScreenDimmingIdle:
        state = Observer::SCREEN_DIMMING_IDLE;
        break;
      default:
        LOG(ERROR) << "Unhandled screen dimming state " << signal_state;
    }
    FOR_EACH_OBSERVER(Observer, observers_, ScreenDimmingRequested(state));
  }

  void SuspendImminentReceived(dbus::Signal* signal) {
    if (!has_suspend_delay_id_) {
      LOG(ERROR) << "Received unrequested "
                 << power_manager::kSuspendImminentSignal << " signal";
      return;
    }

    dbus::MessageReader reader(signal);
    power_manager::SuspendImminent protobuf_imminent;
    if (!reader.PopArrayOfBytesAsProto(&protobuf_imminent)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kSuspendImminentSignal << " signal";
      return;
    }

    if (suspend_is_pending_) {
      LOG(WARNING) << "Got " << power_manager::kSuspendImminentSignal
                   << " signal about pending suspend attempt "
                   << protobuf_imminent.suspend_id() << " while still waiting "
                   << "on attempt " << pending_suspend_id_;
    }

    pending_suspend_id_ = protobuf_imminent.suspend_id();
    suspend_is_pending_ = true;
    num_pending_suspend_readiness_callbacks_ = 0;
    FOR_EACH_OBSERVER(Observer, observers_, SuspendImminent());
    MaybeReportSuspendReadiness();
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
        bool down =
            (proto.type() == power_manager::InputEvent_Type_POWER_BUTTON_DOWN);
        FOR_EACH_OBSERVER(PowerManagerClient::Observer, observers_,
                          PowerButtonEventReceived(down, timestamp));
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

  void SuspendStateChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    power_manager::SuspendState proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Unable to decode protocol buffer from "
                 << power_manager::kSuspendStateChangedSignal << " signal";
      return;
    }

    VLOG(1) << "Got " << power_manager::kSuspendStateChangedSignal << " signal:"
            << " type=" << proto.type() << " wall_time=" << proto.wall_time();
    base::Time wall_time =
        base::Time::FromInternalValue(proto.wall_time());
    switch (proto.type()) {
      case power_manager::SuspendState_Type_SUSPEND_TO_MEMORY:
        last_suspend_wall_time_ = wall_time;
        break;
      case power_manager::SuspendState_Type_RESUME:
        FOR_EACH_OBSERVER(
            PowerManagerClient::Observer, observers_,
            SystemResumed(wall_time - last_suspend_wall_time_));
        break;
    }
  }

  // Registers a suspend delay with the power manager.  This is usually
  // only called at startup, but if the power manager restarts, we need to
  // create a new delay.
  void RegisterSuspendDelay() {
    // Throw out any old delay that was registered.
    suspend_delay_id_ = -1;
    has_suspend_delay_id_ = false;

    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kRegisterSuspendDelayMethod);
    dbus::MessageWriter writer(&method_call);

    power_manager::RegisterSuspendDelayRequest protobuf_request;
    base::TimeDelta timeout =
        base::TimeDelta::FromMilliseconds(kSuspendDelayTimeoutMs);
    protobuf_request.set_timeout(timeout.ToInternalValue());
    protobuf_request.set_description(kSuspendDelayDescription);

    if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
      LOG(ERROR) << "Error constructing message for "
                 << power_manager::kRegisterSuspendDelayMethod;
      return;
    }
    power_manager_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(
            &PowerManagerClientImpl::OnRegisterSuspendDelayReply,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Records the fact that an observer has finished doing asynchronous work
  // that was blocking a pending suspend attempt and possibly reports
  // suspend readiness to powerd.  Called by callbacks returned via
  // GetSuspendReadinessCallback().
  void HandleObserverSuspendReadiness(int32 suspend_id) {
    DCHECK(OnOriginThread());
    if (!suspend_is_pending_ || suspend_id != pending_suspend_id_)
      return;

    num_pending_suspend_readiness_callbacks_--;
    MaybeReportSuspendReadiness();
  }

  // Reports suspend readiness to powerd if no observers are still holding
  // suspend readiness callbacks.
  void MaybeReportSuspendReadiness() {
    if (!suspend_is_pending_ || num_pending_suspend_readiness_callbacks_ > 0)
      return;

    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kHandleSuspendReadinessMethod);
    dbus::MessageWriter writer(&method_call);

    power_manager::SuspendReadinessInfo protobuf_request;
    protobuf_request.set_delay_id(suspend_delay_id_);
    protobuf_request.set_suspend_id(pending_suspend_id_);

    pending_suspend_id_ = -1;
    suspend_is_pending_ = false;

    if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
      LOG(ERROR) << "Error constructing message for "
                 << power_manager::kHandleSuspendReadinessMethod;
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
  int32 suspend_delay_id_;
  bool has_suspend_delay_id_;

  // powerd-supplied ID corresponding to an imminent suspend attempt that is
  // currently being delayed.
  int32 pending_suspend_id_;
  bool suspend_is_pending_;

  // Number of callbacks that have been returned by
  // GetSuspendReadinessCallback() during the currently-pending suspend
  // attempt but have not yet been called.
  int num_pending_suspend_readiness_callbacks_;

  // Wall time from the latest signal telling us that the system was about to
  // suspend to memory.
  base::Time last_suspend_wall_time_;

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
        next_request_id_(1) {
  }

  virtual ~PowerManagerClientStubImpl() {}

  // PowerManagerClient overrides:

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

  virtual void RequestStatusUpdate(UpdateRequestType update_type) OVERRIDE {
    if (update_type == UPDATE_INITIAL) {
      Update();
      return;
    }
    if (!timer_.IsRunning() && update_type == UPDATE_USER) {
      timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(1000),
          this,
          &PowerManagerClientStubImpl::Update);
    } else {
      timer_.Stop();
    }
  }

  virtual void RequestRestart() OVERRIDE {}
  virtual void RequestShutdown() OVERRIDE {}

  virtual void CalculateIdleTime(const CalculateIdleTimeCallback& callback)
      OVERRIDE {
    callback.Run(0);
  }

  virtual void RequestIdleNotification(int64 threshold) OVERRIDE {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PowerManagerClientStubImpl::TriggerIdleNotify,
                   base::Unretained(this),
                   threshold),
        base::TimeDelta::FromMilliseconds(threshold));
  }

  virtual void NotifyUserActivity(
      const base::TimeTicks& last_activity_time) OVERRIDE {}
  virtual void NotifyVideoActivity(
      const base::TimeTicks& last_activity_time,
      bool is_fullscreen) OVERRIDE {}
  virtual void SetPolicy(
      const power_manager::PowerManagementPolicy& policy) OVERRIDE {}
  virtual void RequestPowerStateOverrides(
      uint32 request_id,
      base::TimeDelta duration,
      int overrides,
      const PowerStateRequestIdCallback& callback) OVERRIDE {
    // Mimic the behavior of power manager w.r.t. the request_id.
    if (request_id == 0) {
      callback.Run(next_request_id_++);
    } else {
      callback.Run(request_id);
    }
  }
  virtual void CancelPowerStateOverrides(uint32 request_id) OVERRIDE {}
  virtual void SetIsProjecting(bool is_projecting) OVERRIDE {}
  virtual base::Closure GetSuspendReadinessCallback() OVERRIDE {
    return base::Closure();
  }

 private:
  void Update() {
    if (pause_count_ > 0) {
      pause_count_--;
    } else {
      int discharge_amt = battery_percentage_ <= 10 ? 1 : 10;
      battery_percentage_ += (discharging_ ? -discharge_amt : discharge_amt);
      battery_percentage_ = std::min(std::max(battery_percentage_, 0), 100);
      // We pause at 0 and 100% so that it's easier to check those conditions.
      if (battery_percentage_ == 0 || battery_percentage_ == 100) {
        discharging_ = !discharging_;
        pause_count_ = 4;
      }
    }
    const int kSecondsToEmptyFullBattery(3 * 60 * 60);  // 3 hours.

    status_.is_calculating_battery_time = (pause_count_ > 1);
    status_.line_power_on = !discharging_;
    status_.battery_is_present = true;
    status_.battery_percentage = battery_percentage_;
    status_.battery_seconds_to_empty =
        std::max(1, battery_percentage_ * kSecondsToEmptyFullBattery / 100);
    status_.battery_seconds_to_full =
        std::max(static_cast<int64>(1),
                 kSecondsToEmptyFullBattery - status_.battery_seconds_to_empty);

    status_.averaged_battery_time_to_empty = status_.battery_seconds_to_empty;
    status_.averaged_battery_time_to_full = status_.battery_seconds_to_full;

    FOR_EACH_OBSERVER(Observer, observers_, PowerChanged(status_));
  }

  void SetBrightness(double percent, bool user_initiated) {
    brightness_ = std::min(std::max(0.0, percent), 100.0);
    int brightness_level = static_cast<int>(brightness_);
    FOR_EACH_OBSERVER(Observer, observers_,
                      BrightnessChanged(brightness_level, user_initiated));
  }

  void TriggerIdleNotify(int64 threshold) {
    FOR_EACH_OBSERVER(Observer, observers_, IdleNotify(threshold));
  }

  bool discharging_;
  int battery_percentage_;
  double brightness_;
  int pause_count_;
  ObserverList<Observer> observers_;
  base::RepeatingTimer<PowerManagerClientStubImpl> timer_;
  PowerSupplyStatus status_;
  uint32 next_request_id_;
};

PowerManagerClient::PowerManagerClient() {
}

PowerManagerClient::~PowerManagerClient() {
}

PowerManagerClient* PowerManagerClient::Create(
    DBusClientImplementationType type,
    dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new PowerManagerClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new PowerManagerClientStubImpl();
}

}  // namespace chromeos
