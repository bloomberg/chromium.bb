// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_CHROMIUM_HTTP2_PUSH_PROMISE_INDEX_H_
#define NET_SPDY_CHROMIUM_HTTP2_PUSH_PROMISE_INDEX_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/spdy/chromium/spdy_session_key.h"
#include "url/gurl.h"

namespace net {

class SpdySession;

class NET_EXPORT Http2PushPromiseIndex {
 public:
  Http2PushPromiseIndex();
  ~Http2PushPromiseIndex();

  // If there is a session for |key| that has an unclaimed push stream for
  // |url|, return it.  Otherwise return nullptr.
  base::WeakPtr<SpdySession> Find(const SpdySessionKey& key, const GURL& url);

  // (Un)register a SpdySession with an unclaimed pushed stream for |url|, so
  // that the right SpdySession can be served by FindAvailableSession.
  void RegisterUnclaimedPushedStream(GURL url,
                                     base::WeakPtr<SpdySession> spdy_session);
  void UnregisterUnclaimedPushedStream(const GURL& url,
                                       SpdySession* spdy_session);

 private:
  typedef std::vector<base::WeakPtr<SpdySession>> WeakSessionList;
  typedef std::map<GURL, WeakSessionList> UnclaimedPushedStreamMap;

  // A map of all SpdySessions owned by |this| that have an unclaimed pushed
  // streams for a GURL.  Might contain invalid WeakPtr's.
  // A single SpdySession can only have at most one pushed stream for each GURL,
  // but it is possible that multiple SpdySessions have pushed streams for the
  // same GURL.
  UnclaimedPushedStreamMap unclaimed_pushed_streams_;

  DISALLOW_COPY_AND_ASSIGN(Http2PushPromiseIndex);
};

}  // namespace net

#endif  // NET_SPDY_CHROMIUM_HTTP2_PUSH_PROMISE_INDEX_H_
