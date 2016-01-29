// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CLIENT_SESSION_BASE_H_
#define NET_QUIC_QUIC_CLIENT_SESSION_BASE_H_

#include "base/macros.h"
#include "net/quic/quic_client_promised_info.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_spdy_session.h"

namespace net {

class QuicSpdyClientStream;

// For client/http layer code. Lookup promised streams based on
// matching promised request url. The same map can be shared across
// multiple sessions, since cross-origin pushes are allowed (subject
// to authority constraints).  Clients should use this map to enforce
// session affinity for requests corresponding to cross-origin push
// promised streams.
using QuicPromisedByUrlMap =
    std::unordered_map<std::string, QuicClientPromisedInfo*>;

// Base class for all client-specific QuicSession subclasses.
class NET_EXPORT_PRIVATE QuicClientSessionBase : public QuicSpdySession {
 public:
  // Caller retains ownership of |promised_by_url|.
  QuicClientSessionBase(QuicConnection* connection,
                        QuicPromisedByUrlMap* promised_by_url,
                        const QuicConfig& config);

  ~QuicClientSessionBase() override;

  // Called when the proof in |cached| is marked valid.  If this is a secure
  // QUIC session, then this will happen only after the proof verifier
  // completes.
  virtual void OnProofValid(
      const QuicCryptoClientConfig::CachedState& cached) = 0;

  // Called when proof verification details become available, either because
  // proof verification is complete, or when cached details are used. This
  // will only be called for secure QUIC connections.
  virtual void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& verify_details) = 0;

  // Override base class to set FEC policy before any data is sent by client.
  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;

  // Called by |headers_stream_| when push promise headers have been
  // received for a stream.
  void OnPromiseHeaders(QuicStreamId stream_id,
                        StringPiece headers_data) override;

  // Called by |headers_stream_| when push promise headers have been
  // completely received.  |fin| will be true if the fin flag was set
  // in the headers.
  void OnPromiseHeadersComplete(QuicStreamId stream_id,
                                QuicStreamId promised_stream_id,
                                size_t frame_len) override;

  // Called by |QuicSpdyClientStream| on receipt of response headers,
  // needed to detect promised server push streams, as part of
  // client-request to push-stream rendezvous.
  void OnInitialHeadersComplete(QuicStreamId stream_id,
                                const SpdyHeaderBlock& response_headers);

  // Called by |QuicSpdyClientStream| on receipt of PUSH_PROMISE, does
  // some session level validation and creates the
  // |QuicClientPromisedInfo| inserting into maps by id and url.
  void HandlePromised(QuicStreamId id,
                      std::unique_ptr<SpdyHeaderBlock> headers);

  // Session retains ownership.
  QuicClientPromisedInfo* GetPromisedByUrl(const std::string& url);
  // Session retains ownership.
  QuicClientPromisedInfo* GetPromisedById(const QuicStreamId id);

  // Removes |promised| from the maps by url and id and destroys
  // promised.
  void DeletePromised(QuicClientPromisedInfo* promised);

  // Sends Rst for the stream, and makes sure that future calls to
  // IsClosedStream(id) return true, which ensures that any subsequent
  // frames related to this stream will be ignored (modulo flow
  // control accounting).
  void ResetPromised(QuicStreamId id, QuicRstStreamErrorCode error_code);

  size_t get_max_promises() const {
    return get_max_open_streams() * kMaxPromisedStreamsMultiplier;
  }

 private:
  // For QuicSpdyClientStream to detect that a response corresponds to a
  // promise.
  using QuicPromisedByIdMap =
      std::unordered_map<QuicStreamId, std::unique_ptr<QuicClientPromisedInfo>>;

  // As per rfc7540, section 10.5: track promise streams in "reserved
  // (remote)".  The primary key is URL from he promise request
  // headers.  The promised stream id is a secondary key used to get
  // promise info when the response headers of the promised stream
  // arrive.
  QuicPromisedByUrlMap* promised_by_url_;
  QuicPromisedByIdMap promised_by_id_;
  QuicStreamId largest_promised_stream_id_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientSessionBase);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLIENT_SESSION_BASE_H_
