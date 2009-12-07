// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/socket_stream_host.h"

#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/common/net/socket_stream.h"
#include "chrome/common/render_messages.h"
#include "net/socket_stream/socket_stream.h"

static const char* kSocketHostKey = "socketHost";

class SocketStreamInfo : public net::SocketStream::UserData {
 public:
  explicit SocketStreamInfo(SocketStreamHost* host) : host_(host) {}
  virtual ~SocketStreamInfo() {}
  SocketStreamHost* host() const { return host_; }

 private:
  SocketStreamHost* host_;
};

SocketStreamHost::SocketStreamHost(
    net::SocketStream::Delegate* delegate,
    ResourceDispatcherHost::Receiver* receiver,
    int socket_id)
    : delegate_(delegate),
      receiver_(receiver),
      socket_id_(socket_id) {
  DCHECK_NE(socket_id_, chrome_common_net::kNoSocketId);
  LOG(INFO) << "SocketStreamHost: socket_id=" << socket_id_;
}

/* static */
SocketStreamHost*
SocketStreamHost::GetSocketStreamHost(net::SocketStream* socket) {
  net::SocketStream::UserData* d = socket->GetUserData(kSocketHostKey);
  if (d) {
    SocketStreamInfo* info = static_cast<SocketStreamInfo*>(d);
    return info->host();
  }
  return NULL;
}

SocketStreamHost::~SocketStreamHost() {
  LOG(INFO) << "SocketStreamHost destructed socket_id=" << socket_id_;
  if (!receiver_->Send(new ViewMsg_SocketStream_Closed(socket_id_))) {
    LOG(ERROR) << "ViewMsg_SocketStream_Closed failed.";
  }
  socket_->DetachDelegate();
}

void SocketStreamHost::Connect(const GURL& url) {
  LOG(INFO) << "SocketStreamHost::Connect url=" << url;
  socket_ = new net::SocketStream(url, delegate_);
  URLRequestContextGetter* context_getter = Profile::GetDefaultRequestContext();
  if (context_getter)
      socket_->set_context(context_getter->GetURLRequestContext());
  socket_->SetUserData(kSocketHostKey, new SocketStreamInfo(this));
  socket_->Connect();
}

bool SocketStreamHost::SendData(const std::vector<char>& data) {
  LOG(INFO) << "SocketStreamHost::SendData";
  if (!socket_)
    return false;
  return socket_->SendData(&data[0], data.size());
}

void SocketStreamHost::Close() {
  LOG(INFO) << "SocketStreamHost::Close";
  if (!socket_)
    return;
  return socket_->Close();
}

bool SocketStreamHost::Connected(int max_pending_send_allowed) {
  return receiver_->Send(new ViewMsg_SocketStream_Connected(
      socket_id_, max_pending_send_allowed));
}

bool SocketStreamHost::SentData(int amount_sent) {
  return receiver_->Send(new ViewMsg_SocketStream_SentData(
      socket_id_, amount_sent));
}

bool SocketStreamHost::ReceivedData(const char* data, int len) {
  return receiver_->Send(new ViewMsg_SocketStream_ReceivedData(
      socket_id_, std::vector<char>(data, data + len)));
}
