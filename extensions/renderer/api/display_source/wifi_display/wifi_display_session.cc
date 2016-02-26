// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_session.h"

#include "base/logging.h"
#include "base/timer/timer.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"

namespace {
const char kErrorInternal[] = "An internal error has occurred";
}  // namespace

namespace extensions {

using api::display_source::ErrorType;

WiFiDisplaySession::WiFiDisplaySession(
    const DisplaySourceSessionParams& params)
  : binding_(this),
    params_(params),
    weak_factory_(this) {
  DCHECK(params_.render_frame);
  params.render_frame->GetServiceRegistry()->ConnectToRemoteService(
      mojo::GetProxy(&service_));
  service_.set_connection_error_handler(base::Bind(
          &WiFiDisplaySession::OnConnectionError,
          weak_factory_.GetWeakPtr()));

  service_->SetClient(binding_.CreateInterfacePtrAndBind());
  binding_.set_connection_error_handler(base::Bind(
          &WiFiDisplaySession::OnConnectionError,
          weak_factory_.GetWeakPtr()));
}

WiFiDisplaySession::~WiFiDisplaySession() {
}

void WiFiDisplaySession::Start(const CompletionCallback& callback) {
  DCHECK_EQ(DisplaySourceSession::Idle, state_);
  DCHECK(!terminated_callback_.is_null())
      << "Should be set with 'SetNotificationCallbacks'";
  DCHECK(!error_callback_.is_null())
      << "Should be set with 'SetNotificationCallbacks'";

  service_->Connect(params_.sink_id, params_.auth_method, params_.auth_data);
  state_ = DisplaySourceSession::Establishing;
  start_completion_callback_ = callback;
}

void WiFiDisplaySession::Terminate(const CompletionCallback& callback) {
  DCHECK_EQ(DisplaySourceSession::Established, state_);
  service_->Disconnect();
  state_ = DisplaySourceSession::Terminating;
  teminate_completion_callback_ = callback;
}

void WiFiDisplaySession::OnConnected(const mojo::String& ip_address) {
  DCHECK_EQ(DisplaySourceSession::Established, state_);
  ip_address_ = ip_address;
  // TODO(Mikhail): Start Wi-Fi Display session control message exchange.
}

void WiFiDisplaySession::OnConnectRequestHandled(bool success,
                                                 const mojo::String& error) {
  DCHECK_EQ(DisplaySourceSession::Establishing, state_);
  state_ =
      success ? DisplaySourceSession::Established : DisplaySourceSession::Idle;
  RunStartCallback(success, error);
}

void WiFiDisplaySession::OnTerminated() {
  DCHECK_NE(DisplaySourceSession::Idle, state_);
  state_ = DisplaySourceSession::Idle;
  terminated_callback_.Run();
}

void WiFiDisplaySession::OnDisconnectRequestHandled(bool success,
                                                    const mojo::String& error) {
  RunTerminateCallback(success, error);
}

void WiFiDisplaySession::OnError(int32_t type,
                                 const mojo::String& description) {
  DCHECK(type > api::display_source::ERROR_TYPE_NONE
         && type <= api::display_source::ERROR_TYPE_LAST);
  DCHECK_EQ(DisplaySourceSession::Established, state_);
  error_callback_.Run(static_cast<ErrorType>(type), description);
}

void WiFiDisplaySession::OnMessage(const mojo::String& data) {
  DCHECK_EQ(DisplaySourceSession::Established, state_);
}

void WiFiDisplaySession::OnConnectionError() {
  // We must explicitly notify the session termination as it will never
  // arrive from browser process (IPC is broken).
  switch (state_) {
    case DisplaySourceSession::Idle:
    case DisplaySourceSession::Establishing:
      RunStartCallback(false, kErrorInternal);
      break;
    case DisplaySourceSession::Terminating:
    case DisplaySourceSession::Established:
      error_callback_.Run(api::display_source::ERROR_TYPE_UNKNOWN_ERROR,
                          kErrorInternal);
      state_ = DisplaySourceSession::Idle;
      terminated_callback_.Run();
      break;
    default:
      NOTREACHED();
  }
}

void WiFiDisplaySession::RunStartCallback(bool success,
                                          const std::string& error_message) {
  if (!start_completion_callback_.is_null())
    start_completion_callback_.Run(success, error_message);
}

void WiFiDisplaySession::RunTerminateCallback(
    bool success,
    const std::string& error_message) {
  if (!teminate_completion_callback_.is_null())
    teminate_completion_callback_.Run(success, error_message);
}

}  // namespace extensions
