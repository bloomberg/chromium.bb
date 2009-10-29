// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/socket_stream_host.h"

#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/common/net/socket_stream.h"
#include "net/socket_stream/socket_stream.h"

static const char* kSocketIdKey = "socketId";

class SocketStreamId : public net::SocketStream::UserData {
 public:
  explicit SocketStreamId(int socket_id) : socket_id_(socket_id) {}
  virtual ~SocketStreamId() {}
  int socket_id() const { return socket_id_; }
 private:
  int socket_id_;
};

SocketStreamHost::SocketStreamHost(
    net::SocketStream::Delegate* delegate, int socket_id)
    : delegate_(delegate),
      socket_id_(socket_id) {
  DCHECK_NE(socket_id_, chrome_common_net::kNoSocketId);
  LOG(INFO) << "SocketStreamHost: socket_id=" << socket_id_;
}

/* static */
int SocketStreamHost::SocketIdFromSocketStream(net::SocketStream* socket) {
  net::SocketStream::UserData* d = socket->GetUserData(kSocketIdKey);
  if (d) {
    SocketStreamId* socket_stream_id = static_cast<SocketStreamId*>(d);
    return socket_stream_id->socket_id();
  }
  return chrome_common_net::kNoSocketId;
}

SocketStreamHost::~SocketStreamHost() {
  LOG(INFO) << "SocketStreamHost destructed socket_id=" << socket_id_;
  socket_->DetachDelegate();
}

void SocketStreamHost::Connect(const GURL& url) {
  LOG(INFO) << "SocketStreamHost::Connect url=" << url;
  socket_ = new net::SocketStream(url, delegate_);
  URLRequestContextGetter* context_getter = Profile::GetDefaultRequestContext();
  if (context_getter)
      socket_->set_context(context_getter->GetURLRequestContext());
  socket_->SetUserData(kSocketIdKey, new SocketStreamId(socket_id_));
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
