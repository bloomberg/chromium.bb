// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/media_analytics_client.h"

#include <cstdint>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The MediaAnalyticsClient implementation used in production.
class MediaAnalyticsClientImpl : public MediaAnalyticsClient {
 public:
  MediaAnalyticsClientImpl() : dbus_proxy_(nullptr), weak_ptr_factory_(this) {}

  ~MediaAnalyticsClientImpl() override {}

  void SetMediaPerceptionSignalHandler(
      const MediaPerceptionSignalHandler& handler) override {
    DCHECK(media_perception_signal_handler_.is_null())
        << "MediaPerceptionSignalHandler already set and setting it again.";
    media_perception_signal_handler_ = handler;
  }

  void ClearMediaPerceptionSignalHandler() override {
    media_perception_signal_handler_.Reset();
  }

  void GetState(const StateCallback& callback) override {
    dbus::MethodCall method_call(media_perception::kMediaPerceptionServiceName,
                                 media_perception::kStateFunction);
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaAnalyticsClientImpl::OnState,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void SetState(const mri::State& state,
                const StateCallback& callback) override {
    DCHECK(state.has_status()) << "Attempting to SetState without status set.";

    dbus::MethodCall method_call(media_perception::kMediaPerceptionServiceName,
                                 media_perception::kStateFunction);

    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(state);

    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaAnalyticsClientImpl::OnState,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void GetDiagnostics(const DiagnosticsCallback& callback) override {
    dbus::MethodCall method_call(media_perception::kMediaPerceptionServiceName,
                                 media_perception::kGetDiagnosticsFunction);
    // TODO(lasoren): Verify that this timeout setting is sufficient.
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaAnalyticsClientImpl::OnGetDiagnostics,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    dbus_proxy_ = bus->GetObjectProxy(
        media_perception::kMediaPerceptionServiceName,
        dbus::ObjectPath(media_perception::kMediaPerceptionServicePath));
    // Connect to the MediaPerception signal.
    dbus_proxy_->ConnectToSignal(
        media_perception::kMediaPerceptionInterface,
        media_perception::kDetectionSignal,
        base::Bind(&MediaAnalyticsClientImpl::OnDetectionSignalReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&MediaAnalyticsClientImpl::OnSignalConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded)
        << "Connect to " << interface << " " << signal << " failed.";
  }

  // Handler that is triggered when a MediaPerception proto is received from
  // the media analytics process.
  void OnDetectionSignalReceived(dbus::Signal* signal) {
    if (media_perception_signal_handler_.is_null()) {
      return;
    }

    const uint8_t* bytes = nullptr;
    size_t length = 0;

    dbus::MessageReader reader(signal);

    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Invalid detection signal: " << signal->ToString();
      return;
    }

    mri::MediaPerception media_perception;
    if (!media_perception.ParseFromArray(bytes, length)) {
      LOG(ERROR) << "Failed to parse MediaPerception message.";
      return;
    }

    media_perception_signal_handler_.Run(media_perception);
  }

  void OnState(const StateCallback& callback, dbus::Response* response) {
    mri::State state;
    if (!response) {
      LOG(ERROR) << "Call to State failed to get response.";
      callback.Run(false, state);
      return;
    }

    const uint8_t* bytes = nullptr;
    size_t length = 0;

    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Invalid D-Bus response: " << response->ToString();
      callback.Run(false, state);
      return;
    }

    if (!state.ParseFromArray(bytes, length)) {
      LOG(ERROR) << "Failed to parse State message.";
      callback.Run(false, state);
      return;
    }

    callback.Run(true, state);
  }

  void OnGetDiagnostics(const DiagnosticsCallback& callback,
                        dbus::Response* response) {
    mri::Diagnostics diagnostics;
    if (!response) {
      LOG(ERROR) << "Call to GetDiagnostics failed to get response.";
      callback.Run(false, diagnostics);
      return;
    }

    const uint8_t* bytes = nullptr;
    size_t length = 0;

    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      LOG(ERROR) << "Invalid GetDiagnostics response: " << response->ToString();
      callback.Run(false, diagnostics);
      return;
    }

    if (!diagnostics.ParseFromArray(bytes, length)) {
      LOG(ERROR) << "Failed to parse Diagnostics message.";
      callback.Run(false, diagnostics);
      return;
    }

    callback.Run(true, diagnostics);
  }

  dbus::ObjectProxy* dbus_proxy_;

  // Stores a handler registered for receiving the media_perception.proto byte
  // array.
  MediaPerceptionSignalHandler media_perception_signal_handler_;

  base::WeakPtrFactory<MediaAnalyticsClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaAnalyticsClientImpl);
};

MediaAnalyticsClient::~MediaAnalyticsClient() {}

MediaAnalyticsClient* MediaAnalyticsClient::Create() {
  return new MediaAnalyticsClientImpl;
}

MediaAnalyticsClient::MediaAnalyticsClient() {}

}  // namespace chromeos
