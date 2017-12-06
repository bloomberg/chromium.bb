// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/chromium/http2_push_promise_index.h"

#include <utility>

namespace net {

Http2PushPromiseIndex::Http2PushPromiseIndex() = default;

Http2PushPromiseIndex::~Http2PushPromiseIndex() {
  DCHECK(unclaimed_pushed_streams_.empty());
}

void Http2PushPromiseIndex::FindSession(const SpdySessionKey& key,
                                        const GURL& url,
                                        base::WeakPtr<SpdySession>* session,
                                        SpdyStreamId* stream_id) const {
  DCHECK(!url.is_empty());

  *session = nullptr;
  *stream_id = kNoPushedStreamFound;

  auto it = unclaimed_pushed_streams_.lower_bound(
      UnclaimedPushedStream{url, kNoPushedStreamFound, nullptr});

  if (it == unclaimed_pushed_streams_.end() || it->url != url)
    return;

  DCHECK(url.SchemeIsCryptographic());
  while (it != unclaimed_pushed_streams_.end() && it->url == url) {
    if (it->delegate->ValidatePushedStream(key)) {
      *session = it->delegate->GetWeakPtrToSession();
      *stream_id = it->stream_id;
      it->delegate->OnPushedStreamClaimed(it->url, it->stream_id);
      return;
    }
    ++it;
  }
}

void Http2PushPromiseIndex::RegisterUnclaimedPushedStream(
    const GURL& url,
    SpdyStreamId stream_id,
    Delegate* delegate) {
  DCHECK(!url.is_empty());
  DCHECK(url.SchemeIsCryptographic());
  DCHECK(delegate);

  unclaimed_pushed_streams_.insert(
      UnclaimedPushedStream{url, stream_id, delegate});
}

void Http2PushPromiseIndex::UnregisterUnclaimedPushedStream(
    const GURL& url,
    SpdyStreamId stream_id,
    Delegate* delegate) {
  DCHECK(!url.is_empty());
  DCHECK(url.SchemeIsCryptographic());
  DCHECK(delegate);

  size_t result = unclaimed_pushed_streams_.erase(
      UnclaimedPushedStream{url, stream_id, delegate});
  LOG_IF(DFATAL, result != 1)
      << "Only a previously registered entry can be unregistered.";
}

}  // namespace net
