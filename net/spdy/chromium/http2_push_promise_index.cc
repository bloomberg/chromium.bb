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

base::WeakPtr<SpdySession> Http2PushPromiseIndex::FindSession(
    const SpdySessionKey& key,
    const GURL& url) const {
  DCHECK(!url.is_empty());

  UnclaimedPushedStreamMap::const_iterator it =
      unclaimed_pushed_streams_.lower_bound(std::make_pair(url, nullptr));

  if (it == unclaimed_pushed_streams_.end() || it->first != url) {
    return base::WeakPtr<SpdySession>();
  }

  DCHECK(url.SchemeIsCryptographic());
  while (it != unclaimed_pushed_streams_.end() && it->first == url) {
    if (it->second->ValidatePushedStream(key)) {
      it->second->OnPushedStreamClaimed(url);
      return it->second->GetWeakPtrToSession();
    }
    ++it;
  }

  return base::WeakPtr<SpdySession>();
}

void Http2PushPromiseIndex::RegisterUnclaimedPushedStream(const GURL& url,
                                                          Delegate* delegate) {
  DCHECK(!url.is_empty());
  DCHECK(url.SchemeIsCryptographic());

  unclaimed_pushed_streams_.insert(std::make_pair(url, delegate));
}

void Http2PushPromiseIndex::UnregisterUnclaimedPushedStream(
    const GURL& url,
    Delegate* delegate) {
  DCHECK(!url.is_empty());
  DCHECK(url.SchemeIsCryptographic());

  size_t result =
      unclaimed_pushed_streams_.erase(std::make_pair(url, delegate));
  DCHECK_EQ(1u, result);
}

}  // namespace net
