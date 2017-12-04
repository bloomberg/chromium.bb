// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_CHROMIUM_HTTP2_PUSH_PROMISE_INDEX_H_
#define NET_SPDY_CHROMIUM_HTTP2_PUSH_PROMISE_INDEX_H_

#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/spdy/chromium/spdy_session_key.h"
#include "url/gurl.h"

namespace net {

class SpdySession;

// This class manages cross-origin unclaimed pushed streams (push promises) from
// the receipt of PUSH_PROMISE frame until they are matched to a request.  Each
// SpdySessionPool owns one instance of this class, which then allows requests
// to be matched with a pushed stream regardless of which HTTP/2 connection the
// stream is on.
// Only pushed streams with cryptographic schemes (for example, https) are
// allowed to be shared across connections.  Non-cryptographic scheme pushes
// (for example, http) are fully managed within each SpdySession.
// TODO(bnc): Move unclaimed pushed stream management out of SpdySession,
// regardless of scheme, to avoid redundant bookkeeping and complicated
// interactions between SpdySession::UnclaimedPushedStreamContainer and
// Http2PushPromiseIndex.  https://crbug.com/791054.
class NET_EXPORT Http2PushPromiseIndex {
 public:
  // Interface for validating pushed streams, signaling when a pushed stream is
  // claimed, and generating weak pointer.
  class NET_EXPORT Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Return true if the pushed stream can be used for a request with |key|.
    virtual bool ValidatePushedStream(const SpdySessionKey& key) const = 0;

    // Called when a pushed stream is claimed.
    virtual void OnPushedStreamClaimed(const GURL& url) = 0;

    // Generate weak pointer.
    virtual base::WeakPtr<SpdySession> GetWeakPtrToSession() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  Http2PushPromiseIndex();
  ~Http2PushPromiseIndex();

  // Returns a session with |key| that has an unclaimed push stream for |url| if
  // such exists.  Makes no guarantee on which one it returns if there are
  // multiple.  Returns nullptr if no such session exists.
  base::WeakPtr<SpdySession> FindSession(const SpdySessionKey& key,
                                         const GURL& url) const;

  // (Un)registers a Delegate with an unclaimed pushed stream for |url|.
  // Caller must make sure |delegate| stays valid by unregistering the exact
  // same entry before |delegate| is destroyed.
  void RegisterUnclaimedPushedStream(const GURL& url, Delegate* delegate);
  void UnregisterUnclaimedPushedStream(const GURL& url, Delegate* delegate);

 private:
  using UnclaimedPushedStreamMap = std::set<std::pair<GURL, Delegate*>>;

  // A collection of all unclaimed pushed streams.  Delegate must unregister its
  // streams before destruction, so that all pointers remain valid.  It is
  // possible that multiple Delegates have pushed streams for the same GURL.
  UnclaimedPushedStreamMap unclaimed_pushed_streams_;

  DISALLOW_COPY_AND_ASSIGN(Http2PushPromiseIndex);
};

}  // namespace net

#endif  // NET_SPDY_CHROMIUM_HTTP2_PUSH_PROMISE_INDEX_H_
