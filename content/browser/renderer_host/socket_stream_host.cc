// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/socket_stream_host.h"

#include "base/logging.h"
#include "content/common/socket_stream.h"
#include "net/socket_stream/socket_stream_job.h"

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
    net::SocketStream::Delegate* delegate,
    int socket_id)
    : delegate_(delegate),
      socket_id_(socket_id) {
  DCHECK_NE(socket_id_, content_common::kNoSocketId);
  VLOG(1) << "SocketStreamHost: socket_id=" << socket_id_;
}

/* static */
int SocketStreamHost::SocketIdFromSocketStream(net::SocketStream* socket) {
  net::SocketStream::UserData* d = socket->GetUserData(kSocketIdKey);
  if (d) {
    SocketStreamId* socket_stream_id = static_cast<SocketStreamId*>(d);
    return socket_stream_id->socket_id();
  }
  return content_common::kNoSocketId;
}

SocketStreamHost::~SocketStreamHost() {
  VLOG(1) << "SocketStreamHost destructed socket_id=" << socket_id_;
  socket_->DetachDelegate();
}

void SocketStreamHost::Connect(const GURL& url,
                               net::URLRequestContext* request_context) {
  VLOG(1) << "SocketStreamHost::Connect url=" << url;
  socket_ = net::SocketStreamJob::CreateSocketStreamJob(url, delegate_);
  socket_->set_context(request_context);
  socket_->SetUserData(kSocketIdKey, new SocketStreamId(socket_id_));
  socket_->Connect();
}

bool SocketStreamHost::SendData(const std::vector<char>& data) {
  VLOG(1) << "SocketStreamHost::SendData";
  return socket_ && socket_->SendData(&data[0], data.size());
}

void SocketStreamHost::Close() {
  VLOG(1) << "SocketStreamHost::Close";
  if (!socket_)
    return;
  socket_->Close();
}
