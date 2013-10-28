// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_channel_api.h"

#include "base/json/json_writer.h"
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

// T is an extension dictionary (MessageInfo or ChannelInfo)
template <class T>
std::string ParamToString(const T& info) {
  scoped_ptr<base::DictionaryValue> dict = info.ToValue();
  std::string out;
  base::JSONWriter::Write(dict.get(), &out);
  return out;
}

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
  DVLOG(1) << "Sending message " << ParamToString(message_info)
           << " to channel " << ParamToString(channel_info);
  scoped_ptr<Event> event(new Event(OnMessage::kEventName, results.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
    DispatchEventToExtension(socket->owner_extension_id(), event.Pass());
}

CastChannelAPI::~CastChannelAPI() {}

CastChannelAsyncApiFunction::CastChannelAsyncApiFunction()
  : manager_(NULL), error_(cast_channel::CHANNEL_ERROR_NONE) { }

CastChannelAsyncApiFunction::~CastChannelAsyncApiFunction() { }

bool CastChannelAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<CastSocket>::Get(GetProfile());
  return true;
}

bool CastChannelAsyncApiFunction::Respond() {
  return error_ != cast_channel::CHANNEL_ERROR_NONE;
}

CastSocket* CastChannelAsyncApiFunction::GetSocketOrCompleteWithError(
    int channel_id) {
  CastSocket* socket = GetSocket(channel_id);
  if (!socket) {
    SetResultFromError(cast_channel::CHANNEL_ERROR_INVALID_CHANNEL_ID);
    AsyncWorkCompleted();
  }
  return socket;
}

int CastChannelAsyncApiFunction::AddSocket(CastSocket* socket) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(socket);
  DCHECK(manager_);
  return manager_->Add(socket);
}

void CastChannelAsyncApiFunction::RemoveSocket(int channel_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(manager_);
  manager_->Remove(extension_->id(), channel_id);
}

void CastChannelAsyncApiFunction::SetResultFromSocket(int channel_id) {
  CastSocket* socket = GetSocket(channel_id);
  DCHECK(socket);
  ChannelInfo channel_info;
  socket->FillChannelInfo(&channel_info);
  error_ = socket->error_state();
  SetResultFromChannelInfo(channel_info);
}

void CastChannelAsyncApiFunction::SetResultFromError(ChannelError error) {
  ChannelInfo channel_info;
  channel_info.channel_id = -1;
  channel_info.url = "";
  channel_info.ready_state = cast_channel::READY_STATE_CLOSED;
  channel_info.error_state = error;
  SetResultFromChannelInfo(channel_info);
  error_ = error;
}

CastSocket* CastChannelAsyncApiFunction::GetSocket(int channel_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(manager_);
  return manager_->Get(extension_->id(), channel_id);
}

void CastChannelAsyncApiFunction::SetResultFromChannelInfo(
    const ChannelInfo& channel_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SetResult(channel_info.ToValue().release());
}

CastChannelOpenFunction::CastChannelOpenFunction()
  : new_channel_id_(0) { }

CastChannelOpenFunction::~CastChannelOpenFunction() { }

bool CastChannelOpenFunction::PrePrepare() {
  api_ = CastChannelAPI::Get(GetProfile());
  return CastChannelAsyncApiFunction::PrePrepare();
}

bool CastChannelOpenFunction::Prepare() {
  params_ = Open::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void CastChannelOpenFunction::AsyncWorkStart() {
  DCHECK(api_);
  CastSocket* socket = new CastSocket(extension_->id(), GURL(params_->url),
                                      api_, g_browser_process->net_log());
  new_channel_id_ = AddSocket(socket);
  socket->set_id(new_channel_id_);
  socket->Connect(base::Bind(&CastChannelOpenFunction::OnOpen, this));
}

void CastChannelOpenFunction::OnOpen(int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SetResultFromSocket(new_channel_id_);
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
  CastSocket* socket = GetSocketOrCompleteWithError(
      params_->channel.channel_id);
  if (socket)
    socket->SendMessage(params_->message,
                        base::Bind(&CastChannelSendFunction::OnSend, this));
}

void CastChannelSendFunction::OnSend(int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (result < 0) {
    SetResultFromError(cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  } else {
    SetResultFromSocket(params_->channel.channel_id);
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
  CastSocket* socket = GetSocketOrCompleteWithError(
      params_->channel.channel_id);
  if (socket)
    socket->Close(base::Bind(&CastChannelCloseFunction::OnClose, this));
}

void CastChannelCloseFunction::OnClose(int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "CastChannelCloseFunction::OnClose result = " << result;
  if (result < 0) {
    SetResultFromError(cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  } else {
    int channel_id = params_->channel.channel_id;
    SetResultFromSocket(channel_id);
    RemoveSocket(channel_id);
  }
  AsyncWorkCompleted();
}

}  // namespace extensions
