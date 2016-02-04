// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_push_promise_index.h"

#include "net/quic/quic_client_promised_info.h"
#include "net/quic/spdy_utils.h"

using net::SpdyHeaderBlock;

namespace net {

QuicClientPushPromiseIndex::QuicClientPushPromiseIndex() {}

QuicClientPushPromiseIndex::~QuicClientPushPromiseIndex() {}

QuicClientPushPromiseIndex::TryHandle::~TryHandle() {}

QuicAsyncStatus QuicClientPushPromiseIndex::Try(
    std::unique_ptr<SpdyHeaderBlock> request,
    QuicClientPushPromiseIndex::Delegate* delegate,
    QuicSpdyStream::Visitor* stream_visitor,
    TryHandle** handle) {
  string url(SpdyUtils::GetUrlFromHeaderBlock(*request));
  QuicPromisedByUrlMap::iterator it = promised_by_url_.find(url);
  if (it != promised_by_url_.end()) {
    QuicClientPromisedInfo* promised = it->second;
    QuicAsyncStatus rv = promised->HandleClientRequest(
        std::move(request), delegate, stream_visitor);
    if (rv == QUIC_PENDING) {
      *handle = promised;
    }
    return rv;
  }
  return QUIC_FAILURE;
}

}  // namespace net
