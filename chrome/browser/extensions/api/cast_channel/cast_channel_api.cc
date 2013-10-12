// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_channel_api.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace extensions {

namespace Close = cast_channel::Close;
namespace OnError = cast_channel::OnError;
namespace OnMessage = cast_channel::OnMessage;
namespace Open = cast_channel::Open;
namespace Send = cast_channel::Send;
using cast_channel::CastSocket;
using cast_channel::ChannelError;
using cast_channel::ChannelInfo;
using cast_channel::MessageInfo;
using cast_channel::ReadyState;
using content::BrowserThread;

namespace {
const long kUnknownChannelId = -1;
}  // namespace

CastChannelAPI::CastChannelAPI(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

// static
CastChannelAPI* CastChannelAPI::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<CastChannelAPI>::GetForProfile(profile);
}

static base::LazyInstance<ProfileKeyedAPIFactory<CastChannelAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<CastChannelAPI>* CastChannelAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void CastChannelAPI::OnError(const CastSocket* socket,
                             cast_channel::ChannelError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChannelInfo channel_info;
  socket->FillChannelInfo(&channel_info);
  channel_info.error_state = error;
  scoped_ptr<base::ListValue> results = OnError::Create(channel_info);
  scoped_ptr<Event> event(new Event(OnError::kEventName, results.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
    DispatchEventToExtension(socket->owner_extension_id(), event.Pass());

  // Destroy the socket that caused the error.
  ApiResourceManager<CastSocket>* manager =
    ApiResourceManager<CastSocket>::Get(profile_);
  manager->Remove(socket->owner_extension_id(), socket->id());
}

void CastChannelAPI::OnMessage(const CastSocket* socket,
                               const MessageInfo& message_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChannelInfo channel_info;
  socket->FillChannelInfo(&channel_info);
  scoped_ptr<base::ListValue> results =
    OnMessage::Create(channel_info, message_info);
  scoped_ptr<Event> event(new Event(OnMessage::kEventName, results.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
    DispatchEventToExtension(socket->owner_extension_id(), event.Pass());
}

CastChannelAPI::~CastChannelAPI() {}

CastChannelAsyncApiFunction::CastChannelAsyncApiFunction()
  : socket_(NULL), channel_id_(kUnknownChannelId), manager_(NULL),
    error_(cast_channel::CHANNEL_ERROR_NONE) { }

CastChannelAsyncApiFunction::~CastChannelAsyncApiFunction() { }

bool CastChannelAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<CastSocket>::Get(profile());
  return true;
}

bool CastChannelAsyncApiFunction::Respond() {
  return error_ != cast_channel::CHANNEL_ERROR_NONE;
}

ApiResourceManager<api::cast_channel::CastSocket>*
CastChannelAsyncApiFunction::GetSocketManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return manager_;
}

CastSocket* CastChannelAsyncApiFunction::GetSocket(long channel_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!socket_);
  DCHECK_EQ(channel_id_, kUnknownChannelId);
  CastSocket* socket = GetSocketManager()->Get(extension_->id(), channel_id);
  if (socket) {
    socket_ = socket;
    channel_id_ = channel_id;
  }
  return socket;
}

void CastChannelAsyncApiFunction::RemoveSocketIfError() {
  if (error_ != cast_channel::CHANNEL_ERROR_NONE)
    RemoveSocket();
}

void CastChannelAsyncApiFunction::RemoveSocket() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(socket_);
  DCHECK(channel_id_ != kUnknownChannelId);
  GetSocketManager()->Remove(extension_->id(), channel_id_);
  channel_id_ = kUnknownChannelId;
  socket_ = NULL;
}

void CastChannelAsyncApiFunction::SetResultFromChannelInfo(
    const ChannelInfo& channel_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SetResult(channel_info.ToValue().release());
}

void CastChannelAsyncApiFunction::SetResultFromSocket() {
  DCHECK(socket_);
  ChannelInfo channel_info;
  socket_->FillChannelInfo(&channel_info);
  error_ = socket_->error_state();
  SetResultFromChannelInfo(channel_info);
}

void CastChannelAsyncApiFunction::SetResultFromError(
    const std::string& url, ChannelError error) {
  ChannelInfo channel_info;
  channel_info.channel_id = channel_id_;
  channel_info.url = url;
  channel_info.ready_state = cast_channel::READY_STATE_CLOSED;
  channel_info.error_state = error;
  SetResultFromChannelInfo(channel_info);
  error_ = error;
}

CastChannelOpenFunction::CastChannelOpenFunction() { }

CastChannelOpenFunction::~CastChannelOpenFunction() { }

bool CastChannelOpenFunction::PrePrepare() {
  api_ = CastChannelAPI::Get(profile());
  return CastChannelAsyncApiFunction::PrePrepare();
}

bool CastChannelOpenFunction::Prepare() {
  params_ = Open::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void CastChannelOpenFunction::AsyncWorkStart() {
  DCHECK(api_);
  socket_ = new CastSocket(extension_->id(), GURL(params_->url),
                           api_, g_browser_process->net_log());
  socket_->Connect(base::Bind(&CastChannelOpenFunction::OnOpen, this));
}

void CastChannelOpenFunction::OnOpen(int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (result == net::OK) {
    socket_->set_id(GetSocketManager()->Add(socket_));
    SetResultFromSocket();
  } else {
    SetResultFromError(params_->url,
                       cast_channel::CHANNEL_ERROR_CONNECT_ERROR);
  }
  RemoveSocketIfError();
  AsyncWorkCompleted();
}

CastChannelSendFunction::CastChannelSendFunction() { }

CastChannelSendFunction::~CastChannelSendFunction() { }

bool CastChannelSendFunction::Prepare() {
  params_ = Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void CastChannelSendFunction::AsyncWorkStart() {
  if (!GetSocket(params_->channel.channel_id)) {
    SetResultFromError("",
                       cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
    return;
  }
  socket_->SendMessage(params_->message,
                       base::Bind(&CastChannelSendFunction::OnSend, this));
}

void CastChannelSendFunction::OnSend(int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (result < 0) {
    SetResultFromError("",
                       cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  } else {
    SetResultFromSocket();
  }
  RemoveSocketIfError();
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
  if (!GetSocket(params_->channel.channel_id)) {
    SetResultFromError("", cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
    return;
  }
  socket_->Close(base::Bind(&CastChannelCloseFunction::OnClose, this));
}

void CastChannelCloseFunction::OnClose(int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "CastChannelCloseFunction::OnClose result = " << result;
  if (result < 0) {
    SetResultFromError("", cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
    RemoveSocketIfError();
  } else {
    SetResultFromSocket();
    RemoveSocket();
  }
  AsyncWorkCompleted();
}

}  // namespace extensions
