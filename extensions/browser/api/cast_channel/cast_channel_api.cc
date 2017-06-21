// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_channel_api.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_socket_service_factory.h"
#include "components/cast_channel/keep_alive_delegate.h"
#include "components/cast_channel/logger.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/cast_channel/cast_channel_enum_util.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/browser/event_router.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

// Default timeout interval for connection setup.
// Used if not otherwise specified at ConnectInfo::timeout.
const int kDefaultConnectTimeoutMillis = 5000;  // 5 seconds.

namespace extensions {

namespace Close = api::cast_channel::Close;
namespace OnError = api::cast_channel::OnError;
namespace OnMessage = api::cast_channel::OnMessage;
namespace Open = api::cast_channel::Open;
namespace Send = api::cast_channel::Send;
using api::cast_channel::ChannelInfo;
using api::cast_channel::ConnectInfo;
using api::cast_channel::ErrorInfo;
using api::cast_channel::MessageInfo;
using cast_channel::ChannelError;
using cast_channel::CastDeviceCapability;
using cast_channel::CastMessage;
using cast_channel::CastSocket;
using cast_channel::CastSocketImpl;
using cast_channel::CastTransport;
using cast_channel::KeepAliveDelegate;
using cast_channel::LastError;
using cast_channel::Logger;

using content::BrowserThread;

namespace {

// T is an extension dictionary (MessageInfo or ChannelInfo)
template <class T>
std::string ParamToString(const T& info) {
  std::unique_ptr<base::DictionaryValue> dict = info.ToValue();
  std::string out;
  base::JSONWriter::Write(*dict, &out);
  return out;
}

// Fills |channel_info| from the destination and state of |socket|.
void FillChannelInfo(const CastSocket& socket, ChannelInfo* channel_info) {
  DCHECK(channel_info);
  channel_info->channel_id = socket.id();
  const net::IPEndPoint& ip_endpoint = socket.ip_endpoint();
  channel_info->connect_info.ip_address = ip_endpoint.ToStringWithoutPort();
  channel_info->connect_info.port = ip_endpoint.port();
  channel_info->connect_info.auth =
      api::cast_channel::CHANNEL_AUTH_TYPE_SSL_VERIFIED;
  channel_info->ready_state =
      api::cast_channel::ToReadyState(socket.ready_state());
  channel_info->error_state =
      api::cast_channel::ToChannelError(socket.error_state());
  channel_info->keep_alive = socket.keep_alive();
  channel_info->audio_only = socket.audio_only();
}

// Fills |error_info| from |error_state| and |last_error|.
void FillErrorInfo(api::cast_channel::ChannelError error_state,
                   const LastError& last_error,
                   ErrorInfo* error_info) {
  error_info->error_state = error_state;
  if (last_error.channel_event != cast_channel::ChannelEvent::UNKNOWN)
    error_info->event_type.reset(
        new int(cast_channel::AsInteger(last_error.channel_event)));
  if (last_error.challenge_reply_error !=
      cast_channel::ChallengeReplyError::NONE) {
    error_info->challenge_reply_error_type.reset(
        new int(cast_channel::AsInteger(last_error.challenge_reply_error)));
  }
  if (last_error.net_return_value <= 0)
    error_info->net_return_value.reset(new int(last_error.net_return_value));
}

bool IsValidConnectInfoPort(const ConnectInfo& connect_info) {
  return connect_info.port > 0 && connect_info.port <
    std::numeric_limits<uint16_t>::max();
}

bool IsValidConnectInfoIpAddress(const ConnectInfo& connect_info) {
  net::IPAddress ip_address;
  return ip_address.AssignFromIPLiteral(connect_info.ip_address);
}

}  // namespace

CastChannelAPI::CastChannelAPI(content::BrowserContext* context)
    : browser_context_(context), logger_(new Logger()) {
  DCHECK(browser_context_);
}

// static
CastChannelAPI* CastChannelAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<CastChannelAPI>::Get(context);
}

scoped_refptr<Logger> CastChannelAPI::GetLogger() {
  return logger_;
}

void CastChannelAPI::SendEvent(const std::string& extension_id,
                               std::unique_ptr<Event> event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EventRouter* event_router = EventRouter::Get(GetBrowserContext());
  if (event_router) {
    event_router->DispatchEventToExtension(extension_id, std::move(event));
  }
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<CastChannelAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CastChannelAPI>*
CastChannelAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void CastChannelAPI::SetSocketForTest(
    std::unique_ptr<CastSocket> socket_for_test) {
  socket_for_test_ = std::move(socket_for_test);
}

std::unique_ptr<CastSocket> CastChannelAPI::GetSocketForTest() {
  return std::move(socket_for_test_);
}

content::BrowserContext* CastChannelAPI::GetBrowserContext() const {
  return browser_context_;
}

CastChannelAPI::~CastChannelAPI() {}

CastChannelAsyncApiFunction::CastChannelAsyncApiFunction()
    : cast_socket_service_(nullptr) {}

CastChannelAsyncApiFunction::~CastChannelAsyncApiFunction() { }

bool CastChannelAsyncApiFunction::PrePrepare() {
  cast_socket_service_ =
      cast_channel::CastSocketServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(cast_socket_service_);
  return true;
}

bool CastChannelAsyncApiFunction::Respond() {
  return GetError().empty();
}

void CastChannelAsyncApiFunction::SetResultFromSocket(
    const CastSocket& socket) {
  ChannelInfo channel_info;
  FillChannelInfo(socket, &channel_info);
  api::cast_channel::ChannelError error =
      api::cast_channel::ToChannelError(socket.error_state());
  if (error != api::cast_channel::CHANNEL_ERROR_NONE) {
    SetError("Channel socket error = " + base::IntToString(error));
  }
  SetResultFromChannelInfo(channel_info);
}

void CastChannelAsyncApiFunction::SetResultFromError(
    int channel_id,
    api::cast_channel::ChannelError error) {
  ChannelInfo channel_info;
  channel_info.channel_id = channel_id;
  channel_info.ready_state = api::cast_channel::READY_STATE_CLOSED;
  channel_info.error_state = error;
  channel_info.connect_info.ip_address = "";
  channel_info.connect_info.port = 0;
  channel_info.connect_info.auth =
      api::cast_channel::CHANNEL_AUTH_TYPE_SSL_VERIFIED;
  SetResultFromChannelInfo(channel_info);
  SetError("Channel error = " + base::IntToString(error));
}

void CastChannelAsyncApiFunction::SetResultFromChannelInfo(
    const ChannelInfo& channel_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SetResult(channel_info.ToValue());
}

CastChannelOpenFunction::CastChannelOpenFunction()
    : new_channel_id_(0) {
}

CastChannelOpenFunction::~CastChannelOpenFunction() { }

net::IPEndPoint* CastChannelOpenFunction::ParseConnectInfo(
    const ConnectInfo& connect_info) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(connect_info.ip_address));
  return new net::IPEndPoint(ip_address,
                             static_cast<uint16_t>(connect_info.port));
}

bool CastChannelOpenFunction::PrePrepare() {
  api_ = CastChannelAPI::Get(browser_context());
  return CastChannelAsyncApiFunction::PrePrepare();
}

bool CastChannelOpenFunction::Prepare() {
  params_ = Open::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  const ConnectInfo& connect_info = params_->connect_info;
  if (!IsValidConnectInfoPort(connect_info)) {
    SetError("Invalid connect_info (invalid port)");
  } else if (!IsValidConnectInfoIpAddress(connect_info)) {
    SetError("Invalid connect_info (invalid IP address)");
  } else {
    // Parse timeout parameters if they are set.
    if (connect_info.liveness_timeout.get()) {
      liveness_timeout_ =
          base::TimeDelta::FromMilliseconds(*connect_info.liveness_timeout);
    }
    if (connect_info.ping_interval.get()) {
      ping_interval_ =
          base::TimeDelta::FromMilliseconds(*connect_info.ping_interval);
    }

    // Validate timeout parameters.
    if (liveness_timeout_ < base::TimeDelta() ||
        ping_interval_ < base::TimeDelta()) {
      SetError("livenessTimeout and pingInterval must be greater than 0.");
    } else if ((liveness_timeout_ > base::TimeDelta()) !=
               (ping_interval_ > base::TimeDelta())) {
      SetError("livenessTimeout and pingInterval must be set together.");
    } else if (liveness_timeout_ < ping_interval_) {
      SetError("livenessTimeout must be longer than pingTimeout.");
    }
  }

  if (!GetError().empty()) {
    return false;
  }

  ip_endpoint_.reset(ParseConnectInfo(connect_info));
  return true;
}

void CastChannelOpenFunction::AsyncWorkStart() {
  DCHECK(api_);
  DCHECK(ip_endpoint_.get());
  const ConnectInfo& connect_info = params_->connect_info;
  CastSocket* socket;
  std::unique_ptr<CastSocket> test_socket = api_->GetSocketForTest();
  if (test_socket.get()) {
    socket = test_socket.release();
  } else {
    socket = new CastSocketImpl(
        *ip_endpoint_, ExtensionsBrowserClient::Get()->GetNetLog(),
        base::TimeDelta::FromMilliseconds(connect_info.timeout.get()
                                              ? *connect_info.timeout
                                              : kDefaultConnectTimeoutMillis),
        liveness_timeout_, ping_interval_, api_->GetLogger(),
        connect_info.capabilities.get() ? *connect_info.capabilities
                                        : CastDeviceCapability::NONE);
  }
  new_channel_id_ = cast_socket_service_->AddSocket(base::WrapUnique(socket));

  auto* observer = cast_socket_service_->GetObserver(extension_->id());
  if (!observer) {
    observer = cast_socket_service_->AddObserver(
        extension_->id(), base::MakeUnique<CastMessageHandler>(
                              base::Bind(&CastChannelAPI::SendEvent,
                                         api_->AsWeakPtr(), extension_->id()),
                              api_->GetLogger()));
  }

  socket->AddObserver(observer);
  // Construct read delegates.
  socket->Connect(base::Bind(&CastChannelOpenFunction::OnOpen, this));
}

void CastChannelOpenFunction::OnOpen(ChannelError result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "Connect finished, OnOpen invoked.";
  // TODO: If we failed to open the CastSocket, we may want to clean up here,
  // rather than relying on the extension to call close(). This can be done by
  // calling RemoveSocket() and api_->GetLogger()->ClearLastError(channel_id).
  if (result != ChannelError::UNKNOWN) {
    CastSocket* socket = cast_socket_service_->GetSocket(new_channel_id_);
    CHECK(socket);
    SetResultFromSocket(*socket);
  } else {
    // The socket is being destroyed.
    SetResultFromError(new_channel_id_,
                       api::cast_channel::CHANNEL_ERROR_UNKNOWN);
  }

  AsyncWorkCompleted();
}

CastChannelSendFunction::CastChannelSendFunction() { }

CastChannelSendFunction::~CastChannelSendFunction() { }

bool CastChannelSendFunction::Prepare() {
  params_ = Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  if (params_->message.namespace_.empty()) {
    SetError("message_info.namespace_ is required");
    return false;
  }
  if (params_->message.source_id.empty()) {
    SetError("message_info.source_id is required");
    return false;
  }
  if (params_->message.destination_id.empty()) {
    SetError("message_info.destination_id is required");
    return false;
  }
  switch (params_->message.data->GetType()) {
    case base::Value::Type::STRING:
    case base::Value::Type::BINARY:
      break;
    default:
      SetError("Invalid type of message_info.data");
      return false;
  }
  return true;
}

void CastChannelSendFunction::AsyncWorkStart() {
  CastSocket* socket =
      cast_socket_service_->GetSocket(params_->channel.channel_id);
  if (!socket) {
    SetResultFromError(params_->channel.channel_id,
                       api::cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
    return;
  }
  CastMessage message_to_send;
  if (!MessageInfoToCastMessage(params_->message, &message_to_send)) {
    SetResultFromError(params_->channel.channel_id,
                       api::cast_channel::CHANNEL_ERROR_INVALID_MESSAGE);
    AsyncWorkCompleted();
    return;
  }
  socket->transport()->SendMessage(
      message_to_send, base::Bind(&CastChannelSendFunction::OnSend, this));
}

void CastChannelSendFunction::OnSend(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int channel_id = params_->channel.channel_id;
  CastSocket* socket = cast_socket_service_->GetSocket(channel_id);
  if (result < 0 || !socket) {
    SetResultFromError(channel_id,
                       api::cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  } else {
    SetResultFromSocket(*socket);
  }
  AsyncWorkCompleted();
}

CastChannelCloseFunction::CastChannelCloseFunction() { }

CastChannelCloseFunction::~CastChannelCloseFunction() { }

bool CastChannelCloseFunction::PrePrepare() {
  api_ = CastChannelAPI::Get(browser_context());
  return CastChannelAsyncApiFunction::PrePrepare();
}

bool CastChannelCloseFunction::Prepare() {
  params_ = Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void CastChannelCloseFunction::AsyncWorkStart() {
  CastSocket* socket =
      cast_socket_service_->GetSocket(params_->channel.channel_id);
  if (!socket) {
    SetResultFromError(params_->channel.channel_id,
                       api::cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
  } else {
    socket->Close(base::Bind(&CastChannelCloseFunction::OnClose, this));
  }
}

void CastChannelCloseFunction::OnClose(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "CastChannelCloseFunction::OnClose result = " << result;
  int channel_id = params_->channel.channel_id;
  CastSocket* socket = cast_socket_service_->GetSocket(channel_id);
  if (result < 0 || !socket) {
    SetResultFromError(channel_id,
                       api::cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  } else {
    SetResultFromSocket(*socket);
    // This will delete |socket|.
    cast_socket_service_->RemoveSocket(channel_id);
    api_->GetLogger()->ClearLastError(channel_id);
  }
  AsyncWorkCompleted();
}

CastChannelOpenFunction::CastMessageHandler::CastMessageHandler(
    const EventDispatchCallback& ui_dispatch_cb,
    scoped_refptr<Logger> logger)
    : ui_dispatch_cb_(ui_dispatch_cb), logger_(logger) {
  DCHECK(logger_);
}

CastChannelOpenFunction::CastMessageHandler::~CastMessageHandler() {
}

void CastChannelOpenFunction::CastMessageHandler::OnError(
    const cast_channel::CastSocket& socket,
    ChannelError error_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ChannelInfo channel_info;
  FillChannelInfo(socket, &channel_info);
  channel_info.error_state = api::cast_channel::ToChannelError(error_state);
  ErrorInfo error_info;
  FillErrorInfo(channel_info.error_state, logger_->GetLastError(socket.id()),
                &error_info);

  std::unique_ptr<base::ListValue> results =
      OnError::Create(channel_info, error_info);
  std::unique_ptr<Event> event(new Event(
      events::CAST_CHANNEL_ON_ERROR, OnError::kEventName, std::move(results)));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(ui_dispatch_cb_, base::Passed(std::move(event))));
}

void CastChannelOpenFunction::CastMessageHandler::OnMessage(
    const cast_channel::CastSocket& socket,
    const CastMessage& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  MessageInfo message_info;
  CastMessageToMessageInfo(message, &message_info);
  ChannelInfo channel_info;
  FillChannelInfo(socket, &channel_info);
  VLOG(1) << "Received message " << ParamToString(message_info)
          << " on channel " << ParamToString(channel_info);

  std::unique_ptr<base::ListValue> results =
      OnMessage::Create(channel_info, message_info);
  std::unique_ptr<Event> event(new Event(events::CAST_CHANNEL_ON_MESSAGE,
                                         OnMessage::kEventName,
                                         std::move(results)));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(ui_dispatch_cb_, base::Passed(std::move(event))));
}

}  // namespace extensions
