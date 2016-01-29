// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CLIENT_PROMISED_INFO_H_
#define NET_QUIC_QUIC_CLIENT_PROMISED_INFO_H_

#include <sys/types.h>
#include <string>

#include "net/quic/quic_alarm.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_spdy_stream.h"
#include "net/spdy/spdy_framer.h"

namespace net {

class QuicClientSessionBase;
class QuicConnectionHelperInterface;

namespace tools {
namespace test {
class QuicClientPromisedInfoPeer;
}  // namespace test
}  // namespace tools

// QuicClientPromisedInfo tracks the client state of a server push
// stream from the time a PUSH_PROMISE is received until rendezvous
// between the promised response and the corresponding client request
// is complete.
class NET_EXPORT_PRIVATE QuicClientPromisedInfo {
 public:
  // Interface to QuicSpdyClientStream
  QuicClientPromisedInfo(QuicClientSessionBase* session,
                         QuicStreamId id,
                         std::string url);
  ~QuicClientPromisedInfo();

  void Init();

  // Validate promise headers etc.
  void OnPromiseHeaders(std::unique_ptr<SpdyHeaderBlock> request_headers);

  // Store response, trigger listener |OnResponse| above.
  void OnResponseHeaders(std::unique_ptr<SpdyHeaderBlock> response_headers);

  // Interface to client/http layer code.

  // Client requests are initially associated to promises by matching
  // URL in the client request against the URL in the promise headers.
  // The push can be cross-origin, so the client should validate that
  // the session is authoritative for the promised URL.  If not, it
  // should call |RejectUnauthorized|.
  QuicClientSessionBase* session() { return session_; }

  void RejectUnauthorized() { Reset(QUIC_UNAUTHORIZED_PROMISE_URL); }
  // If the promised response contains Vary header, then the fields
  // specified by Vary must match between the client request header
  // and the promise headers (see https://crbug.com//554220).  Vary
  // validation requires the response headers (for the actual Vary
  // field list), the promise headers (taking the role of the "cached"
  // request), and the client request headers.
  SpdyHeaderBlock* request_headers() { return request_headers_.get(); }

  SpdyHeaderBlock* response_headers() { return response_headers_.get(); }
  // After validation, client will use this to access the pushed
  // stream.

  QuicStreamId id() const { return id_; }

  const std::string url() const { return url_; }

  // The initial match by URL can happen any time after a promise has
  // been received, including before the promised response that is
  // required for validation (response_headers() == nullptr), in which
  // case the client must use the listener to be notified when the
  // response becomes available.
  class Listener {
   public:
    virtual ~Listener() {}
    // Invoked when response headers for the promised stream are
    // available.  Also fires if the pushed stream has failed before
    // response became available, for example due to a timeout waiting
    // for the stream.  In the failure case, calling
    // response_headers() will return nullptr.
    virtual void OnResponse() = 0;
  };

  void SetListener(Listener* listener) { listener_ = listener; }

 private:
  friend class net::tools::test::QuicClientPromisedInfoPeer;

  class CleanupAlarm : public QuicAlarm::Delegate {
   public:
    explicit CleanupAlarm(QuicClientPromisedInfo* promised)
        : promised_(promised) {}

    QuicTime OnAlarm() override;

    QuicClientPromisedInfo* promised_;
  };

  void MaybeNotifyListener();

  void Reset(QuicRstStreamErrorCode error_code);

  QuicClientSessionBase* session_;
  QuicConnectionHelperInterface* helper_;
  QuicStreamId id_;
  std::string url_;
  std::unique_ptr<SpdyHeaderBlock> request_headers_;
  std::unique_ptr<SpdyHeaderBlock> response_headers_;
  Listener* listener_;
  // The promise will commit suicide eventually if it is not claimed
  // by a GET first.
  std::unique_ptr<QuicAlarm> cleanup_alarm_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientPromisedInfo);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLIENT_PROMISED_INFO_H_
