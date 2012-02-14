// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/update_engine_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {

// Returns UPDATE_STATUS_ERROR on error.
UpdateEngineClient::UpdateStatusOperation UpdateStatusFromString(
    const std::string& str) {
  if (str == "UPDATE_STATUS_IDLE")
    return UpdateEngineClient::UPDATE_STATUS_IDLE;
  if (str == "UPDATE_STATUS_CHECKING_FOR_UPDATE")
    return UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE;
  if (str == "UPDATE_STATUS_UPDATE_AVAILABLE")
    return UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE;
  if (str == "UPDATE_STATUS_DOWNLOADING")
    return UpdateEngineClient::UPDATE_STATUS_DOWNLOADING;
  if (str == "UPDATE_STATUS_VERIFYING")
    return UpdateEngineClient::UPDATE_STATUS_VERIFYING;
  if (str == "UPDATE_STATUS_FINALIZING")
    return UpdateEngineClient::UPDATE_STATUS_FINALIZING;
  if (str == "UPDATE_STATUS_UPDATED_NEED_REBOOT")
    return UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
  if (str == "UPDATE_STATUS_REPORTING_ERROR_EVENT")
    return UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT;
  return UpdateEngineClient::UPDATE_STATUS_ERROR;
}

// Used in UpdateEngineClient::EmptyUpdateCheckCallback().
void EmptyUpdateCheckCallbackBody(
    UpdateEngineClient::UpdateCheckResult unused_result) {
}

}  // namespace

// The UpdateEngineClient implementation used in production.
class UpdateEngineClientImpl : public UpdateEngineClient {
 public:
  explicit UpdateEngineClientImpl(dbus::Bus* bus)
      : update_engine_proxy_(NULL),
        weak_ptr_factory_(this),
        last_status_() {
    update_engine_proxy_ = bus->GetObjectProxy(
        update_engine::kUpdateEngineServiceName,
        update_engine::kUpdateEngineServicePath);

    // Monitor the D-Bus signal for brightness changes. Only the power
    // manager knows the actual brightness level. We don't cache the
    // brightness level in Chrome as it will make things less reliable.
    update_engine_proxy_->ConnectToSignal(
        update_engine::kUpdateEngineInterface,
        update_engine::kStatusUpdate,
        base::Bind(&UpdateEngineClientImpl::StatusUpdateReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&UpdateEngineClientImpl::StatusUpdateConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~UpdateEngineClientImpl() {
  }

  // UpdateEngineClient override.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  // UpdateEngineClient override.
  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // UpdateEngineClient override.
  virtual bool HasObserver(Observer* observer) OVERRIDE {
    return observers_.HasObserver(observer);
  }

  // UpdateEngineClient override.
  virtual void RequestUpdateCheck(UpdateCheckCallback callback) OVERRIDE {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kAttemptUpdate);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("");  // Unused.
    writer.AppendString("");  // Unused.

    VLOG(1) << "Requesting an update check";
    update_engine_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&UpdateEngineClientImpl::OnRequestUpdateCheck,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // UpdateEngineClient override.
  virtual void RebootAfterUpdate() OVERRIDE {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kRebootIfNeeded);

    VLOG(1) << "Requesting a reboot";
    update_engine_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&UpdateEngineClientImpl::OnRebootAfterUpdate,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // UpdateEngineClient override.
  virtual void SetReleaseTrack(const std::string& track) OVERRIDE {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kSetTrack);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(track);

    VLOG(1) << "Requesting to set the release track to " << track;
    update_engine_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&UpdateEngineClientImpl::OnSetReleaseTrack,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // UpdateEngineClient override.
  virtual void GetReleaseTrack(GetReleaseTrackCallback callback) OVERRIDE {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kGetTrack);

    VLOG(1) << "Requesting to get the current release track";
    update_engine_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&UpdateEngineClientImpl::OnGetReleaseTrack,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // UpdateEngineClient override.
  virtual Status GetLastStatus() OVERRIDE {
    return last_status_;
  }

 private:
  // Called when a response for RequestUpdateCheck() is received.
  void OnRequestUpdateCheck(UpdateCheckCallback callback,
                            dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request update check";
      callback.Run(UPDATE_RESULT_FAILED);
      return;
    }
    callback.Run(UPDATE_RESULT_SUCCESS);
  }

  // Called when a response for RebootAfterUpdate() is received.
  void OnRebootAfterUpdate(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request rebooting after update";
      return;
    }
  }

  // Called when a response for SetReleaseTrack() is received.
  void OnSetReleaseTrack(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request setting release track";
      return;
    }
  }

  // Called when a response for GetReleaseTrack() is received.
  void OnGetReleaseTrack(GetReleaseTrackCallback callback,
                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request getting release track";
      callback.Run("");
      return;
    }
    dbus::MessageReader reader(response);
    std::string release_track;
    if (!reader.PopString(&release_track)) {
      LOG(ERROR) << "Incorrect response: " << response->ToString();
      callback.Run("");
      return;
    }
    VLOG(1) << "The current release track received: " << release_track;
    callback.Run(release_track);
  }

  // Called when a status update signal is received.
  void StatusUpdateReceived(dbus::Signal* signal) {
    VLOG(1) << "Status update signal received: " << signal->ToString();
    dbus::MessageReader reader(signal);
    int64 last_checked_time = 0;
    double progress = 0.0;
    std::string current_operation;
    std::string new_version;
    int64_t new_size = 0;
    if (!(reader.PopInt64(&last_checked_time) &&
          reader.PopDouble(&progress) &&
          reader.PopString(&current_operation) &&
          reader.PopString(&new_version) &&
          reader.PopInt64(&new_size))) {
      LOG(ERROR) << "Status changed signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    Status status;
    status.last_checked_time = last_checked_time;
    status.download_progress = progress;
    status.status = UpdateStatusFromString(current_operation);
    status.new_version = new_version;
    status.new_size = new_size;

    last_status_ = status;
    FOR_EACH_OBSERVER(Observer, observers_, UpdateStatusChanged(status));
  }

  // Called when the status update signal is initially connected.
  void StatusUpdateConnected(const std::string& interface_name,
                             const std::string& signal_name,
                             bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to status updated signal.";
  }

  dbus::ObjectProxy* update_engine_proxy_;
  ObserverList<Observer> observers_;
  base::WeakPtrFactory<UpdateEngineClientImpl> weak_ptr_factory_;
  Status last_status_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClientImpl);
};

// The UpdateEngineClient implementation used on Linux desktop,
// which does nothing.
class UpdateEngineClientStubImpl : public UpdateEngineClient {
  // UpdateEngineClient overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual bool HasObserver(Observer* observer) OVERRIDE { return false; }

  virtual void RequestUpdateCheck(UpdateCheckCallback callback) OVERRIDE {
    callback.Run(UPDATE_RESULT_FAILED);
  }
  virtual void RebootAfterUpdate() OVERRIDE {}
  virtual void SetReleaseTrack(const std::string& track) OVERRIDE {}
  virtual void GetReleaseTrack(GetReleaseTrackCallback callback) OVERRIDE {
    callback.Run("beta-channel");
  }
  virtual Status GetLastStatus() OVERRIDE { return Status(); }
};

UpdateEngineClient::UpdateEngineClient() {
}

UpdateEngineClient::~UpdateEngineClient() {
}

// static
UpdateEngineClient::UpdateCheckCallback
UpdateEngineClient::EmptyUpdateCheckCallback() {
  return base::Bind(&EmptyUpdateCheckCallbackBody);
}

// static
UpdateEngineClient* UpdateEngineClient::Create(dbus::Bus* bus) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    return new UpdateEngineClientImpl(bus);
  } else {
    return new UpdateEngineClientStubImpl();
  }
}

}  // namespace chromeos
