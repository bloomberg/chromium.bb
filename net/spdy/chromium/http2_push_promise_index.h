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
#include "net/spdy/core/spdy_protocol.h"
#include "url/gurl.h"

namespace net {

class SpdySession;

namespace test {

class Http2PushPromiseIndexPeer;

}  // namespace test

// This value is returned by Http2PushPromiseIndex::FindSession() and
// UnclaimedPushedStreamContainer::FindStream() if no stream is found.
// TODO(bnc): Move UnclaimedPushedStreamContainer::FindStream() to this class.
// https://crbug.com/791054
const SpdyStreamId kNoPushedStreamFound = 0;

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
    virtual void OnPushedStreamClaimed(const GURL& url,
                                       SpdyStreamId stream_id) = 0;

    // Generate weak pointer.
    virtual base::WeakPtr<SpdySession> GetWeakPtrToSession() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  Http2PushPromiseIndex();
  ~Http2PushPromiseIndex();

  // If there exists a session compatible with |key| that has an unclaimed push
  // stream for |url|, then sets |*session| and |*stream| to one such session
  // and stream.  Makes no guarantee on which (session, stream_id) pair it
  // returns if there are multiple matches.  Sets |*session| to nullptr and
  // |*stream| to kNoPushedStreamFound if no such session exists.
  void FindSession(const SpdySessionKey& key,
                   const GURL& url,
                   base::WeakPtr<SpdySession>* session,
                   SpdyStreamId* stream_id) const;

  // (Un)registers a Delegate with an unclaimed pushed stream for |url|.
  // Caller must make sure |delegate| stays valid by unregistering the exact
  // same entry before |delegate| is destroyed.
  void RegisterUnclaimedPushedStream(const GURL& url,
                                     SpdyStreamId stream_id,
                                     Delegate* delegate);
  void UnregisterUnclaimedPushedStream(const GURL& url,
                                       SpdyStreamId stream_id,
                                       Delegate* delegate);

 private:
  friend test::Http2PushPromiseIndexPeer;

  // An unclaimed pushed stream entry.
  struct UnclaimedPushedStream {
    GURL url;
    SpdyStreamId stream_id;
    Delegate* delegate;
  };

  // Function object satisfying the requirements of "Compare", see
  // http://en.cppreference.com/w/cpp/concept/Compare.
  // Used for sorting entries by URL.  Among entries with identical URL, put the
  // one with |stream_id == kNoPushedStreamFound| first, so that it can be used
  // with lower_bound() to search for the first entry with given URL.  For
  // entries with identical URL and stream ID, sort by |delegate| memory
  // address.
  struct CompareByUrl {
    bool operator()(const UnclaimedPushedStream& a,
                    const UnclaimedPushedStream& b) const {
      if (a.url < b.url)
        return true;
      if (a.url > b.url)
        return false;
      // Identical URL.
      if (a.stream_id == kNoPushedStreamFound &&
          b.stream_id != kNoPushedStreamFound) {
        return true;
      }
      if (a.stream_id != kNoPushedStreamFound &&
          b.stream_id == kNoPushedStreamFound) {
        return false;
      }
      if (a.stream_id < b.stream_id)
        return true;
      if (a.stream_id > b.stream_id)
        return false;
      // Identical URL and stream ID.
      return a.delegate < b.delegate;
    }
  };

  // A collection of all unclaimed pushed streams.  Delegate must unregister its
  // streams before destruction, so that all pointers remain valid.  It is
  // possible that multiple Delegates have pushed streams for the same GURL.
  std::set<UnclaimedPushedStream, CompareByUrl> unclaimed_pushed_streams_;

  DISALLOW_COPY_AND_ASSIGN(Http2PushPromiseIndex);
};

}  // namespace net

#endif  // NET_SPDY_CHROMIUM_HTTP2_PUSH_PROMISE_INDEX_H_
