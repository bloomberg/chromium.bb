// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_channel_api.h"

#include <limits>
#include <string>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/cast_channel/logging.pb.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

// Default timeout interval for connection setup.
// Used if not otherwise specified at ConnectInfo::timeout.
const int kDefaultConnectTimeoutMillis = 5000;  // 5 seconds.

namespace extensions {

namespace Close = cast_channel::Close;
namespace OnError = cast_channel::OnError;
namespace OnMessage = cast_channel::OnMessage;
namespace Open = cast_channel::Open;
namespace Send = cast_channel::Send;
using cast_channel::CastSocket;
using cast_channel::ChannelAuthType;
using cast_channel::ChannelError;
using cast_channel::ChannelInfo;
using cast_channel::ConnectInfo;
using cast_channel::ErrorInfo;
using cast_channel::LastErrors;
using cast_channel::Logger;
using cast_channel::MessageInfo;
using cast_channel::ReadyState;
using content::BrowserThread;

namespace {

// T is an extension dictionary (MessageInfo or ChannelInfo)
template <class T>
std::string ParamToString(const T& info) {
  scoped_ptr<base::DictionaryValue> dict = info.ToValue();
  std::string out;
  base::JSONWriter::Write(dict.get(), &out);
  return out;
}

// Fills |channel_info| from the destination and state of |socket|.
void FillChannelInfo(const CastSocket& socket, ChannelInfo* channel_info) {
  DCHECK(channel_info);
  channel_info->channel_id = socket.id();
  channel_info->url = socket.CastUrl();
  const net::IPEndPoint& ip_endpoint = socket.ip_endpoint();
  channel_info->connect_info.ip_address = ip_endpoint.ToStringWithoutPort();
  channel_info->connect_info.port = ip_endpoint.port();
  channel_info->connect_info.auth = socket.channel_auth();
  channel_info->ready_state = socket.ready_state();
  channel_info->error_state = socket.error_state();
}

// Fills |error_info| from |error_state| and |last_errors|.
void FillErrorInfo(ChannelError error_state,
                   const LastErrors& last_errors,
                   ErrorInfo* error_info) {
  error_info->error_state = error_state;
  if (last_errors.event_type != cast_channel::proto::EVENT_TYPE_UNKNOWN)
    error_info->event_type.reset(new int(last_errors.event_type));
  if (last_errors.challenge_reply_error_type !=
      cast_channel::proto::CHALLENGE_REPLY_ERROR_NONE) {
    error_info->challenge_reply_error_type.reset(
        new int(last_errors.challenge_reply_error_type));
  }
  if (last_errors.net_return_value <= 0)
    error_info->net_return_value.reset(new int(last_errors.net_return_value));
  if (last_errors.nss_error_code < 0)
    error_info->nss_error_code.reset(new int(last_errors.nss_error_code));
}

bool IsValidConnectInfoPort(const ConnectInfo& connect_info) {
  return connect_info.port > 0 && connect_info.port <
    std::numeric_limits<uint16_t>::max();
}

bool IsValidConnectInfoAuth(const ConnectInfo& connect_info) {
  return connect_info.auth == cast_channel::CHANNEL_AUTH_TYPE_SSL_VERIFIED ||
    connect_info.auth == cast_channel::CHANNEL_AUTH_TYPE_SSL;
}

bool IsValidConnectInfoIpAddress(const ConnectInfo& connect_info) {
  net::IPAddressNumber ip_address;
  return net::ParseIPLiteralToNumber(connect_info.ip_address, &ip_address);
}

}  // namespace

CastChannelAPI::CastChannelAPI(content::BrowserContext* context)
    : browser_context_(context),
      logger_(
          new Logger(scoped_ptr<base::TickClock>(new base::DefaultTickClock),
                     base::TimeTicks::UnixEpoch())) {
  DCHECK(browser_context_);
}

// static
CastChannelAPI* CastChannelAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<CastChannelAPI>::Get(context);
}

scoped_refptr<Logger> CastChannelAPI::GetLogger() {
  return logger_;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<CastChannelAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CastChannelAPI>*
CastChannelAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void CastChannelAPI::SetSocketForTest(scoped_ptr<CastSocket> socket_for_test) {
  socket_for_test_ = socket_for_test.Pass();
}

scoped_ptr<cast_channel::CastSocket> CastChannelAPI::GetSocketForTest() {
  return socket_for_test_.Pass();
}

void CastChannelAPI::OnError(const CastSocket* socket,
                             cast_channel::ChannelError error_state,
                             const cast_channel::LastErrors& last_errors) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ChannelInfo channel_info;
  FillChannelInfo(*socket, &channel_info);
  channel_info.error_state = error_state;
  ErrorInfo error_info;
  FillErrorInfo(error_state, last_errors, &error_info);
  scoped_ptr<base::ListValue> results =
      OnError::Create(channel_info, error_info);
  scoped_ptr<Event> event(new Event(OnError::kEventName, results.Pass()));
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(socket->owner_extension_id(), event.Pass());
}

void CastChannelAPI::OnMessage(const CastSocket* socket,
                               const MessageInfo& message_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ChannelInfo channel_info;
  FillChannelInfo(*socket, &channel_info);
  scoped_ptr<base::ListValue> results =
    OnMessage::Create(channel_info, message_info);
  VLOG(1) << "Sending message " << ParamToString(message_info)
          << " to channel " << ParamToString(channel_info);
  scoped_ptr<Event> event(new Event(OnMessage::kEventName, results.Pass()));
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(socket->owner_extension_id(), event.Pass());
}

CastChannelAPI::~CastChannelAPI() {}

CastChannelAsyncApiFunction::CastChannelAsyncApiFunction()
  : manager_(NULL), error_(cast_channel::CHANNEL_ERROR_NONE) { }

CastChannelAsyncApiFunction::~CastChannelAsyncApiFunction() { }

bool CastChannelAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<CastSocket>::Get(browser_context());
  return true;
}

bool CastChannelAsyncApiFunction::Respond() {
  return error_ == cast_channel::CHANNEL_ERROR_NONE;
}

CastSocket* CastChannelAsyncApiFunction::GetSocketOrCompleteWithError(
    int channel_id) {
  CastSocket* socket = GetSocket(channel_id);
  if (!socket) {
    SetResultFromError(channel_id,
                       cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
  }
  return socket;
}

int CastChannelAsyncApiFunction::AddSocket(CastSocket* socket) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(socket);
  DCHECK(manager_);
  const int id = manager_->Add(socket);
  socket->set_id(id);
  return id;
}

void CastChannelAsyncApiFunction::RemoveSocket(int channel_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(manager_);
  manager_->Remove(extension_->id(), channel_id);
}

void CastChannelAsyncApiFunction::SetResultFromSocket(
    const CastSocket& socket) {
  ChannelInfo channel_info;
  FillChannelInfo(socket, &channel_info);
  error_ = socket.error_state();
  SetResultFromChannelInfo(channel_info);
}

void CastChannelAsyncApiFunction::SetResultFromError(int channel_id,
                                                     ChannelError error) {
  ChannelInfo channel_info;
  channel_info.channel_id = channel_id;
  channel_info.url = "";
  channel_info.ready_state = cast_channel::READY_STATE_CLOSED;
  channel_info.error_state = error;
  channel_info.connect_info.ip_address = "";
  channel_info.connect_info.port = 0;
  channel_info.connect_info.auth = cast_channel::CHANNEL_AUTH_TYPE_SSL;
  SetResultFromChannelInfo(channel_info);
  error_ = error;
}

CastSocket* CastChannelAsyncApiFunction::GetSocket(int channel_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(manager_);
  return manager_->Get(extension_->id(), channel_id);
}

void CastChannelAsyncApiFunction::SetResultFromChannelInfo(
    const ChannelInfo& channel_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SetResult(channel_info.ToValue().release());
}

CastChannelOpenFunction::CastChannelOpenFunction()
  : new_channel_id_(0) { }

CastChannelOpenFunction::~CastChannelOpenFunction() { }

// TODO(mfoltz): Remove URL parsing when clients have converted to use
// ConnectInfo.

// Allowed schemes for Cast device URLs.
const char kCastInsecureScheme[] = "cast";
const char kCastSecureScheme[] = "casts";

bool CastChannelOpenFunction::ParseChannelUrl(const GURL& url,
                                              ConnectInfo* connect_info) {
  DCHECK(connect_info);
  VLOG(2) << "ParseChannelUrl";
  bool auth_required = false;
  if (url.SchemeIs(kCastSecureScheme)) {
    auth_required = true;
  } else if (!url.SchemeIs(kCastInsecureScheme)) {
    return false;
  }
  // TODO(mfoltz): Test for IPv6 addresses.  Brackets or no brackets?
  // TODO(mfoltz): Maybe enforce restriction to IPv4 private and IPv6
  // link-local networks
  const std::string& path = url.path();
  // Shortest possible: //A:B
  if (path.size() < 5) {
    return false;
  }
  if (path.find("//") != 0) {
    return false;
  }
  size_t colon = path.find_last_of(':');
  if (colon == std::string::npos || colon < 3 || colon > path.size() - 2) {
    return false;
  }
  const std::string& ip_address_str = path.substr(2, colon - 2);
  const std::string& port_str = path.substr(colon + 1);
  VLOG(2) << "IP: " << ip_address_str << " Port: " << port_str;
  int port;
  if (!base::StringToInt(port_str, &port))
    return false;
  connect_info->ip_address = ip_address_str;
  connect_info->port = port;
  connect_info->auth = auth_required ?
    cast_channel::CHANNEL_AUTH_TYPE_SSL_VERIFIED :
    cast_channel::CHANNEL_AUTH_TYPE_SSL;
  return true;
}

net::IPEndPoint* CastChannelOpenFunction::ParseConnectInfo(
    const ConnectInfo& connect_info) {
  net::IPAddressNumber ip_address;
  CHECK(net::ParseIPLiteralToNumber(connect_info.ip_address, &ip_address));
  return new net::IPEndPoint(ip_address, connect_info.port);
}

bool CastChannelOpenFunction::PrePrepare() {
  api_ = CastChannelAPI::Get(browser_context());
  return CastChannelAsyncApiFunction::PrePrepare();
}

bool CastChannelOpenFunction::Prepare() {
  params_ = Open::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  // The connect_info parameter may be a string URL like cast:// or casts:// or
  // a ConnectInfo object.
  std::string cast_url;
  switch (params_->connect_info->GetType()) {
    case base::Value::TYPE_STRING:
      CHECK(params_->connect_info->GetAsString(&cast_url));
      connect_info_.reset(new ConnectInfo);
      if (!ParseChannelUrl(GURL(cast_url), connect_info_.get())) {
        connect_info_.reset();
        SetError("Invalid connect_info (invalid Cast URL " + cast_url + ")");
      }
      break;
    case base::Value::TYPE_DICTIONARY:
      connect_info_ = ConnectInfo::FromValue(*(params_->connect_info));
      if (!connect_info_.get()) {
        SetError("connect_info.auth is required");
      }
      break;
    default:
      SetError("Invalid connect_info (unknown type)");
      break;
  }
  if (!connect_info_.get()) {
    return false;
  }
  if (!IsValidConnectInfoPort(*connect_info_)) {
    SetError("Invalid connect_info (invalid port)");
  } else if (!IsValidConnectInfoAuth(*connect_info_)) {
    SetError("Invalid connect_info (invalid auth)");
  } else if (!IsValidConnectInfoIpAddress(*connect_info_)) {
    SetError("Invalid connect_info (invalid IP address)");
  }
  if (!GetError().empty()) {
    return false;
  }
  channel_auth_ = connect_info_->auth;
  ip_endpoint_.reset(ParseConnectInfo(*connect_info_));
  return true;
}

void CastChannelOpenFunction::AsyncWorkStart() {
  DCHECK(api_);
  DCHECK(ip_endpoint_.get());
  scoped_ptr<CastSocket> socket = api_->GetSocketForTest();
  if (!socket.get()) {
    socket.reset(new CastSocket(
        extension_->id(),
        *ip_endpoint_,
        channel_auth_,
        api_,
        ExtensionsBrowserClient::Get()->GetNetLog(),
        base::TimeDelta::FromMilliseconds(connect_info_->timeout.get()
                                              ? *connect_info_->timeout
                                              : kDefaultConnectTimeoutMillis),
        api_->GetLogger()));
  }
  new_channel_id_ = AddSocket(socket.release());
  CastSocket* new_socket = GetSocket(new_channel_id_);
  api_->GetLogger()->LogNewSocketEvent(*new_socket);
  new_socket->Connect(base::Bind(&CastChannelOpenFunction::OnOpen, this));
}

void CastChannelOpenFunction::OnOpen(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "Connect finished, OnOpen invoked.";
  CastSocket* socket = GetSocket(new_channel_id_);
  if (!socket) {
    SetResultFromError(new_channel_id_,
                       cast_channel::CHANNEL_ERROR_CONNECT_ERROR);
  } else {
    SetResultFromSocket(*socket);
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
    case base::Value::TYPE_STRING:
    case base::Value::TYPE_BINARY:
      break;
    default:
      SetError("Invalid type of message_info.data");
      return false;
  }
  return true;
}

void CastChannelSendFunction::AsyncWorkStart() {
  CastSocket* socket = GetSocket(params_->channel.channel_id);
  if (!socket) {
    SetResultFromError(params_->channel.channel_id,
                       cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
    return;
  }
  socket->SendMessage(params_->message,
                      base::Bind(&CastChannelSendFunction::OnSend, this));
}

void CastChannelSendFunction::OnSend(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int channel_id = params_->channel.channel_id;
  CastSocket* socket = GetSocket(channel_id);
  if (result < 0 || !socket) {
    SetResultFromError(channel_id,
                       cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  } else {
    SetResultFromSocket(*socket);
  }
  AsyncWorkCompleted();
}

CastChannelCloseFunction::CastChannelCloseFunction() { }

CastChannelCloseFunction::~CastChannelCloseFunction() { }

bool CastChannelCloseFunction::Prepare() {
  params_ = Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void CastChannelCloseFunction::AsyncWorkStart() {
  CastSocket* socket = GetSocket(params_->channel.channel_id);
  if (!socket) {
    SetResultFromError(params_->channel.channel_id,
                       cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
  } else {
    socket->Close(base::Bind(&CastChannelCloseFunction::OnClose, this));
  }
}

void CastChannelCloseFunction::OnClose(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "CastChannelCloseFunction::OnClose result = " << result;
  int channel_id = params_->channel.channel_id;
  CastSocket* socket = GetSocket(channel_id);
  if (result < 0 || !socket) {
    SetResultFromError(channel_id,
                       cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  } else {
    SetResultFromSocket(*socket);
    // This will delete |socket|.
    RemoveSocket(channel_id);
    socket = NULL;
  }
  AsyncWorkCompleted();
}

CastChannelGetLogsFunction::CastChannelGetLogsFunction() {
}

CastChannelGetLogsFunction::~CastChannelGetLogsFunction() {
}

bool CastChannelGetLogsFunction::PrePrepare() {
  api_ = CastChannelAPI::Get(browser_context());
  return CastChannelAsyncApiFunction::PrePrepare();
}

bool CastChannelGetLogsFunction::Prepare() {
  return true;
}

void CastChannelGetLogsFunction::AsyncWorkStart() {
  DCHECK(api_);

  size_t length = 0;
  scoped_ptr<char[]> out = api_->GetLogger()->GetLogs(&length);
  if (out.get()) {
    SetResult(new base::BinaryValue(out.Pass(), length));
  } else {
    SetError("Unable to get logs.");
  }

  api_->GetLogger()->Reset();

  AsyncWorkCompleted();
}

}  // namespace extensions
