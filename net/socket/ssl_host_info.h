// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_HOST_INFO_H
#define NET_SOCKET_SSL_HOST_INFO_H

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

// SSLHostInfo is an interface for fetching information about an SSL server.
// This information may be stored on disk so does not include keys or session
// information etc. Primarily it's intended for caching the server's
// certificates.
class SSLHostInfo {
 public:
  SSLHostInfo();
  virtual ~SSLHostInfo();

  // Start will commence the lookup. This must be called before any other
  // methods. By opportunistically calling this early, it may be possible to
  // overlap this object's lookup and reduce latency.
  virtual void Start() = 0;

  // WaitForDataReady returns OK if the fetch of the requested data has
  // completed. Otherwise it returns ERR_IO_PENDING and will call |callback| on
  // the current thread when ready.
  //
  // Only a single callback can be outstanding at a given time and, in the
  // event that WaitForDataReady returns OK, it's the caller's responsibility
  // to delete |callback|.
  //
  // |callback| may be NULL, in which case ERR_IO_PENDING may still be returned
  // but, obviously, a callback will never be made.
  virtual int WaitForDataReady(CompletionCallback* callback) = 0;

  // Persist allows for the host information to be updated for future users.
  // This is a fire and forget operation: the caller may drop its reference
  // from this object and the store operation will still complete. This can
  // only be called once WaitForDataReady has returned OK or called its
  // callback.
  virtual void Persist() = 0;

  struct State {
    // certs is a vector of DER encoded X.509 certificates, as the server
    // returned them and in the same order.
    std::vector<std::string> certs;
    // server_hello contains the bytes of the ServerHello message (or may be
    // empty if the server doesn't support Snap Start.)
    std::string server_hello;
    // npn_valid is true iff |npn_status| and |npn_protocol| is successful.
    bool npn_valid;
    // these members contain the NPN result of a connection to the server.
    SSLClientSocket::NextProtoStatus npn_status;
    std::string npn_protocol;
  };

  // Once the data is ready, it can be read using the following members. These
  // members can then be updated before calling |Persist|.
  const State& state() const;
  State* mutable_state();

 protected:
  // Parse parses an opaque blob of data and fills out the public member fields
  // of this object. It returns true iff the parse was successful. The public
  // member fields will be set to something sane in any case.
  bool Parse(const std::string& data);
  std::string Serialize() const;
  State state_;
};

class SSLHostInfoFactory {
 public:
  virtual ~SSLHostInfoFactory();

  // GetForHost returns a fresh, allocated SSLHostInfo for the given hostname
  // or NULL on failure.
  virtual SSLHostInfo* GetForHost(const std::string& hostname) = 0;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_HOST_INFO_H
