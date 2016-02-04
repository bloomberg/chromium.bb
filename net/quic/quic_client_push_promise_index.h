// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CLIENT_PUSH_PROMISE_INDEX_H_
#define NET_QUIC_QUIC_CLIENT_PUSH_PROMISE_INDEX_H_

#include "net/quic/quic_client_session_base.h"

#include "net/quic/quic_types.h"

namespace net {

class QuicDataToResend;

// QuicClientPushPromiseIndex is the interface to support rendezvous
// between client requests and resources delivered via server push.
// The same index can be shared across multiple sessions (e.g. for the
// same browser users profile), since cross-origin pushes are allowed
// (subject to authority constraints).

class NET_EXPORT_PRIVATE QuicClientPushPromiseIndex {
 public:
  // Delegate is used to complete the rendezvous that began with
  // |Try()|.
  class Delegate {
   public:
    virtual ~Delegate(){};

    // The primary lookup matched request with push promise by URL.  A
    // secondary match is necessary to ensure Vary (RFC 2616, 14.14)
    // is honored.  If Vary is not present, return true.  If Vary is
    // present, return whether designated header fields of
    // |promise_request| and |client_request| match.
    virtual bool CheckVary(const SpdyHeaderBlock& client_request,
                           const SpdyHeaderBlock& promise_request,
                           const SpdyHeaderBlock& promise_response) = 0;

    // Provides the stream that has the promised response.  Returns
    // nullptr if the promise has been removed (due failing validation
    // or other stream error), the client should retry.  Callee does
    // not inherit ownership of |stream|.
    virtual void OnResponse(QuicSpdyStream* stream) = 0;
  };

  class NET_EXPORT_PRIVATE TryHandle {
   public:
    // Cancel the request.
    virtual void Cancel() = 0;

   protected:
    TryHandle() {}
    ~TryHandle();

   private:
    DISALLOW_COPY_AND_ASSIGN(TryHandle);
  };

  QuicClientPushPromiseIndex();
  virtual ~QuicClientPushPromiseIndex();

  // Called by client code, to initiate rendezvous between a request
  // and a server push stream.  If |request|'s url is in the index,
  // rendezvous will be attempted and may complete immediately or
  // asynchronously.  If the matching promise and response headers
  // have already arrived, the delegates methods will fired when |Try|
  // returns and the rendezvous is complete.  Returns |QUIC_SUCCESS|
  // if the rendezvous was a success. Returns |QUIC_FAILURE| if there
  // was no matching promise, or if there was but the rendezvous has
  // failed.  Returns QUIC_PENDING if a matching promise was found,
  // but the rendezvous needs to complete asynchronously because the
  // promised response headers are not yet available.  If result is
  // QUIC_PENDING, then |*handle| will set so that the caller may
  // cancel the request if need be.  The caller does not inherit
  // ownership of |*handle|, and it will not be valid once
  // |delegate->OnReponse()| has fired.
  QuicAsyncStatus Try(std::unique_ptr<SpdyHeaderBlock> request,
                      Delegate* delegate,
                      QuicSpdyStream::Visitor* visitor,
                      TryHandle** handle);

  QuicPromisedByUrlMap* promised_by_url() { return &promised_by_url_; }

 private:
  QuicPromisedByUrlMap promised_by_url_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientPushPromiseIndex);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLIENT_PUSH_PROMISE_INDEX_H_
