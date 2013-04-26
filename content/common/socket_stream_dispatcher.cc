// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/socket_stream_dispatcher.h"

#include <vector>

#include "base/bind.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "content/common/child_thread.h"
#include "content/common/socket_stream.h"
#include "content/common/socket_stream_handle_data.h"
#include "content/common/socket_stream_messages.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "webkit/glue/websocketstreamhandle_bridge.h"
#include "webkit/glue/websocketstreamhandle_delegate.h"

namespace content {

// IPCWebSocketStreamHandleBridge is owned by each SocketStreamHandle.
// It communicates with the main browser process via SocketStreamDispatcher.
class IPCWebSocketStreamHandleBridge
    : public webkit_glue::WebSocketStreamHandleBridge {
 public:
  IPCWebSocketStreamHandleBridge(
      ChildThread* child_thread,
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate)
      : socket_id_(kNoSocketId),
        child_thread_(child_thread),
        handle_(handle),
        delegate_(delegate) {}

  // Returns the handle having given id or NULL if there is no such handle.
  static IPCWebSocketStreamHandleBridge* FromSocketId(int id);

  // webkit_glue::WebSocketStreamHandleBridge methods.
  virtual void Connect(const GURL& url) OVERRIDE;
  virtual bool Send(const std::vector<char>& data) OVERRIDE;
  virtual void Close() OVERRIDE;

  // Called by SocketStreamDispatcher.
  void OnConnected(int max_amount_send_allowed);
  void OnSentData(int amount_sent);
  void OnReceivedData(const std::vector<char>& data);
  void OnClosed();
  void OnFailed(int error_code, const char* error_msg);

 private:
  virtual ~IPCWebSocketStreamHandleBridge();

  void DoConnect(const GURL& url);
  void DoClose();

  // The ID for this bridge and corresponding SocketStream instance in the
  // browser process.
  int socket_id_;

  ChildThread* child_thread_;
  WebKit::WebSocketStreamHandle* handle_;
  webkit_glue::WebSocketStreamHandleDelegate* delegate_;

  // Map from ID to bridge instance.
  static base::LazyInstance<IDMap<IPCWebSocketStreamHandleBridge> >::Leaky
      all_bridges;
};

// static
base::LazyInstance<IDMap<IPCWebSocketStreamHandleBridge> >::Leaky
    IPCWebSocketStreamHandleBridge::all_bridges = LAZY_INSTANCE_INITIALIZER;

/* static */
IPCWebSocketStreamHandleBridge* IPCWebSocketStreamHandleBridge::FromSocketId(
    int id) {
  return all_bridges.Get().Lookup(id);
}

IPCWebSocketStreamHandleBridge::~IPCWebSocketStreamHandleBridge() {
  DVLOG(1) << "Bridge (" << this << ", socket_id_=" << socket_id_
           << ") Destructor";

  if (socket_id_ == kNoSocketId)
    return;

  child_thread_->Send(new SocketStreamHostMsg_Close(socket_id_));
  socket_id_ = kNoSocketId;
}

void IPCWebSocketStreamHandleBridge::Connect(const GURL& url) {
  DCHECK(child_thread_);
  DVLOG(1) << "Bridge (" << this << ") Connect (url=" << url << ")";

  child_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&IPCWebSocketStreamHandleBridge::DoConnect, this, url));
}

bool IPCWebSocketStreamHandleBridge::Send(
    const std::vector<char>& data) {
  DVLOG(1) << "Bridge #" << socket_id_ << " Send (" << data.size()
           << " bytes)";

  if (child_thread_->Send(
      new SocketStreamHostMsg_SendData(socket_id_, data))) {
    if (delegate_)
      delegate_->WillSendData(handle_, &data[0], data.size());
    return true;
  }
  return false;
}

void IPCWebSocketStreamHandleBridge::Close() {
  DVLOG(1) << "Bridge #" << socket_id_ << " Close";

  AddRef();  // Released in DoClose().
  child_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&IPCWebSocketStreamHandleBridge::DoClose, this));
}

void IPCWebSocketStreamHandleBridge::OnConnected(int max_pending_send_allowed) {
  DVLOG(1) << "Bridge #" << socket_id_
           << " OnConnected (max_pending_send_allowed="
           << max_pending_send_allowed << ")";

  if (delegate_)
    delegate_->DidOpenStream(handle_, max_pending_send_allowed);
}

void IPCWebSocketStreamHandleBridge::OnSentData(int amount_sent) {
  DVLOG(1) << "Bridge #" << socket_id_ << " OnSentData (" << amount_sent
           << " bytes)";

  if (delegate_)
    delegate_->DidSendData(handle_, amount_sent);
}

void IPCWebSocketStreamHandleBridge::OnReceivedData(
    const std::vector<char>& data) {
  DVLOG(1) << "Bridge #" << socket_id_ << " OnReceiveData (" << data.size()
           << " bytes)";
  if (delegate_)
    delegate_->DidReceiveData(handle_, &data[0], data.size());
}

void IPCWebSocketStreamHandleBridge::OnClosed() {
  DVLOG(1) << "Bridge #" << socket_id_ << " OnClosed";

  if (socket_id_ != kNoSocketId) {
    all_bridges.Get().Remove(socket_id_);
    socket_id_ = kNoSocketId;
  }
  if (delegate_)
    delegate_->DidClose(handle_);
  delegate_ = NULL;
  Release();
}

void IPCWebSocketStreamHandleBridge::OnFailed(int error_code,
                                              const char* error_msg) {
  DVLOG(1) << "Bridge #" << socket_id_ << " OnFailed (error_code=" << error_code
           << ")";
  if (delegate_)
    delegate_->DidFail(handle_, error_code, ASCIIToUTF16(error_msg));
}

void IPCWebSocketStreamHandleBridge::DoConnect(const GURL& url) {
  DCHECK(child_thread_);
  DCHECK_EQ(socket_id_, kNoSocketId);
  if (delegate_)
    delegate_->WillOpenStream(handle_, url);

  socket_id_ = all_bridges.Get().Add(this);
  DCHECK_NE(socket_id_, kNoSocketId);
  int render_view_id = MSG_ROUTING_NONE;
  const SocketStreamHandleData* data =
      SocketStreamHandleData::ForHandle(handle_);
  if (data)
    render_view_id = data->render_view_id();
  AddRef();  // Released in OnClosed().
  if (child_thread_->Send(
      new SocketStreamHostMsg_Connect(render_view_id, url, socket_id_))) {
    DVLOG(1) << "Bridge #" << socket_id_ << " sent IPC Connect";
    // TODO(ukai): timeout to OnConnected.
  } else {
    DLOG(ERROR) << "Bridge #" << socket_id_ << " failed to send IPC Connect";
    OnClosed();
  }
}

void IPCWebSocketStreamHandleBridge::DoClose() {
  child_thread_->Send(new SocketStreamHostMsg_Close(socket_id_));
  Release();
}

SocketStreamDispatcher::SocketStreamDispatcher() {
}

/* static */
webkit_glue::WebSocketStreamHandleBridge*
SocketStreamDispatcher::CreateBridge(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate) {
  return new IPCWebSocketStreamHandleBridge(
      ChildThread::current(), handle, delegate);
}

bool SocketStreamDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SocketStreamDispatcher, msg)
    IPC_MESSAGE_HANDLER(SocketStreamMsg_Connected, OnConnected)
    IPC_MESSAGE_HANDLER(SocketStreamMsg_SentData, OnSentData)
    IPC_MESSAGE_HANDLER(SocketStreamMsg_ReceivedData, OnReceivedData)
    IPC_MESSAGE_HANDLER(SocketStreamMsg_Closed, OnClosed)
    IPC_MESSAGE_HANDLER(SocketStreamMsg_Failed, OnFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SocketStreamDispatcher::OnConnected(int socket_id,
                                         int max_pending_send_allowed) {
  DVLOG(1) << "SocketStreamDispatcher::OnConnected (max_pending_send_allowed="
           << max_pending_send_allowed << ") to socket_id=" << socket_id;

  IPCWebSocketStreamHandleBridge* bridge =
      IPCWebSocketStreamHandleBridge::FromSocketId(socket_id);
  if (bridge)
    bridge->OnConnected(max_pending_send_allowed);
  else
    DLOG(ERROR) << "No bridge for socket_id=" << socket_id;
}

void SocketStreamDispatcher::OnSentData(int socket_id, int amount_sent) {
  DVLOG(1) << "SocketStreamDispatcher::OnSentData (" << amount_sent
           << " bytes) to socket_id=" << socket_id;

  IPCWebSocketStreamHandleBridge* bridge =
      IPCWebSocketStreamHandleBridge::FromSocketId(socket_id);
  if (bridge)
    bridge->OnSentData(amount_sent);
  else
    DLOG(ERROR) << "No bridge for socket_id=" << socket_id;
}

void SocketStreamDispatcher::OnReceivedData(
    int socket_id, const std::vector<char>& data) {
  DVLOG(1) << "SocketStreamDispatcher::OnReceivedData (" << data.size()
           << " bytes) to socket_id=" << socket_id;

  IPCWebSocketStreamHandleBridge* bridge =
      IPCWebSocketStreamHandleBridge::FromSocketId(socket_id);
  if (bridge)
    bridge->OnReceivedData(data);
  else
    DLOG(ERROR) << "No bridge for socket_id=" << socket_id;
}

void SocketStreamDispatcher::OnClosed(int socket_id) {
  DVLOG(1) << "SocketStreamDispatcher::OnClosed to socket_id=" << socket_id;

  IPCWebSocketStreamHandleBridge* bridge =
      IPCWebSocketStreamHandleBridge::FromSocketId(socket_id);
  if (bridge)
    bridge->OnClosed();
  else
    DLOG(ERROR) << "No bridge for socket_id=" << socket_id;
}

void SocketStreamDispatcher::OnFailed(int socket_id, int error_code) {
  IPCWebSocketStreamHandleBridge* bridge =
      IPCWebSocketStreamHandleBridge::FromSocketId(socket_id);
  if (bridge)
    bridge->OnFailed(error_code, net::ErrorToString(error_code));
  else
    DLOG(ERROR) << "No bridge for socket_id=" << socket_id;
}

}  // namespace content
