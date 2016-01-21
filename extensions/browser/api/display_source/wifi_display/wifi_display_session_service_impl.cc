// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/display_source/wifi_display/wifi_display_session_service_impl.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate_factory.h"

namespace {
const char kErrorCannotHaveMultipleSessions[] =
    "Multiple Wi-Fi Display sessions are not supported";
const char kErrorSinkNotAvailable[] = "The sink is not available";
}  // namespace

namespace extensions {

using namespace api::display_source;

WiFiDisplaySessionServiceImpl::WiFiDisplaySessionServiceImpl(
    DisplaySourceConnectionDelegate* delegate,
    mojo::InterfaceRequest<WiFiDisplaySessionService> request)
    : binding_(this, std::move(request)),
      delegate_(delegate),
      last_connected_sink_(DisplaySourceConnectionDelegate::kInvalidSinkId),
      own_sink_(DisplaySourceConnectionDelegate::kInvalidSinkId),
      weak_factory_(this) {
  delegate_->AddObserver(this);

  auto connection = delegate_->connection();
  if (connection)
    last_connected_sink_ = connection->GetConnectedSink()->id;
}

WiFiDisplaySessionServiceImpl::~WiFiDisplaySessionServiceImpl() {
  delegate_->RemoveObserver(this);
  if (own_sink_ != DisplaySourceConnectionDelegate::kInvalidSinkId)
    Disconnect();
}

// static
void WiFiDisplaySessionServiceImpl::BindToRequest(
    content::BrowserContext* browser_context,
    mojo::InterfaceRequest<WiFiDisplaySessionService> request) {
  DisplaySourceConnectionDelegate* delegate =
      DisplaySourceConnectionDelegateFactory::GetForBrowserContext(
          browser_context);
  CHECK(delegate);

  new WiFiDisplaySessionServiceImpl(delegate, std::move(request));
}

void WiFiDisplaySessionServiceImpl::SetClient(
    WiFiDisplaySessionServiceClientPtr client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = std::move(client);
  client_.set_connection_error_handler(
      base::Bind(&WiFiDisplaySessionServiceImpl::OnClientConnectionError,
                 weak_factory_.GetWeakPtr()));
}

void WiFiDisplaySessionServiceImpl::Connect(int32_t sink_id,
                                            int32_t auth_method,
                                            const mojo::String& auth_data) {
  DCHECK(client_);
  // We support only one Wi-Fi Display session at a time.
  if (delegate_->connection()) {
    client_->OnError(sink_id, ERROR_TYPE_EXCEEDED_SESSION_LIMIT_ERROR,
                     kErrorCannotHaveMultipleSessions);
    return;
  }

  const DisplaySourceSinkInfoList& sinks = delegate_->last_found_sinks();
  auto found = std::find_if(
      sinks.begin(), sinks.end(),
      [sink_id](DisplaySourceSinkInfoPtr ptr) { return ptr->id == sink_id; });
  if (found == sinks.end() || (*found)->state != SINK_STATE_DISCONNECTED) {
    client_->OnError(sink_id, ERROR_TYPE_ESTABLISH_CONNECTION_ERROR,
                     kErrorSinkNotAvailable);
    return;
  }
  AuthenticationInfo auth_info;
  if (auth_method != AUTHENTICATION_METHOD_NONE) {
    DCHECK(auth_method <= AUTHENTICATION_METHOD_LAST);
    auth_info.method = static_cast<AuthenticationMethod>(auth_method);
    auth_info.data = scoped_ptr<std::string>(new std::string(auth_data));
  }
  auto on_error = base::Bind(&WiFiDisplaySessionServiceImpl::OnConnectFailed,
                             weak_factory_.GetWeakPtr(), sink_id);
  delegate_->Connect(sink_id, auth_info, on_error);
  own_sink_ = sink_id;
}

void WiFiDisplaySessionServiceImpl::Disconnect() {
  if (own_sink_ == DisplaySourceConnectionDelegate::kInvalidSinkId) {
    // The connection might drop before this call has arrived.
    // Renderer should have been notified already.
    return;
  }
  DCHECK(delegate_->connection());
  DCHECK_EQ(own_sink_, delegate_->connection()->GetConnectedSink()->id);
  auto on_error = base::Bind(&WiFiDisplaySessionServiceImpl::OnDisconnectFailed,
                             weak_factory_.GetWeakPtr(), own_sink_);
  delegate_->Disconnect(on_error);
}

void WiFiDisplaySessionServiceImpl::SendMessage(const mojo::String& message) {
  if (own_sink_ == DisplaySourceConnectionDelegate::kInvalidSinkId) {
    // The connection might drop before this call has arrived.
    return;
  }
  auto connection = delegate_->connection();
  DCHECK(connection);
  DCHECK_EQ(own_sink_, connection->GetConnectedSink()->id);
  connection->SendMessage(message);
}

void WiFiDisplaySessionServiceImpl::OnSinkMessage(const std::string& message) {
  DCHECK(delegate_->connection());
  DCHECK_NE(own_sink_, DisplaySourceConnectionDelegate::kInvalidSinkId);
  DCHECK(client_);
  client_->OnMessage(message);
}

void WiFiDisplaySessionServiceImpl::OnSinksUpdated(
    const DisplaySourceSinkInfoList& sinks) {
  auto connection = delegate_->connection();
  if (!connection &&
      last_connected_sink_ != DisplaySourceConnectionDelegate::kInvalidSinkId) {
    if (last_connected_sink_ == own_sink_)
      own_sink_ = DisplaySourceConnectionDelegate::kInvalidSinkId;
    if (client_)
      client_->OnDisconnected(last_connected_sink_);
    last_connected_sink_ = DisplaySourceConnectionDelegate::kInvalidSinkId;
  }
  if (connection &&
      last_connected_sink_ != connection->GetConnectedSink()->id) {
    last_connected_sink_ = connection->GetConnectedSink()->id;
    if (client_)
      client_->OnConnected(last_connected_sink_, connection->GetLocalAddress());
    if (last_connected_sink_ == own_sink_) {
      auto on_message =
          base::Bind(&WiFiDisplaySessionServiceImpl::OnSinkMessage,
                     weak_factory_.GetWeakPtr());
      connection->SetMessageReceivedCallback(on_message);
    }
  }
}

void WiFiDisplaySessionServiceImpl::OnConnectionError(
    int sink_id,
    DisplaySourceErrorType type,
    const std::string& description) {
  DCHECK(client_);
  client_->OnError(sink_id, type, description);
}

void WiFiDisplaySessionServiceImpl::OnConnectFailed(
    int sink_id,
    const std::string& message) {
  DCHECK(client_);
  client_->OnError(sink_id, ERROR_TYPE_ESTABLISH_CONNECTION_ERROR, message);
  own_sink_ = DisplaySourceConnectionDelegate::kInvalidSinkId;
}

void WiFiDisplaySessionServiceImpl::OnDisconnectFailed(
    int sink_id,
    const std::string& message) {
  DCHECK(client_);
  client_->OnError(sink_id, ERROR_TYPE_CONNECTION_ERROR, message);
}

void WiFiDisplaySessionServiceImpl::OnClientConnectionError() {
  DLOG(ERROR) << "IPC connection error";
  delete this;
}

}  // namespace extensions
