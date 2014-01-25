// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_QUIC_SERVER_INFO_H_
#define NET_QUIC_CRYPTO_QUIC_SERVER_INFO_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

namespace net {

class X509Certificate;

// QuicServerInfo is an interface for fetching information about a QUIC server.
// This information may be stored on disk so does not include keys or other
// sensitive information. Primarily it's intended for caching the QUIC server's
// crypto config.
class NET_EXPORT_PRIVATE QuicServerInfo {
 public:
  QuicServerInfo(const std::string& hostname);
  virtual ~QuicServerInfo();

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
  virtual int WaitForDataReady(const CompletionCallback& callback) = 0;

  // Persist allows for the server information to be updated for future users.
  // This is a fire and forget operation: the caller may drop its reference
  // from this object and the store operation will still complete. This can
  // only be called once WaitForDataReady has returned OK or called its
  // callback.
  virtual void Persist() = 0;

  struct State {
    State();
    ~State();

    void Clear();

    // TODO(rtenneti): figure out what are the data members.
    std::vector<std::string> data;

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
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

 private:
  // ParseInner is a helper function for Parse.
  bool ParseInner(const std::string& data);

  // This is the QUIC server hostname for which we restore the crypto_config.
  const std::string hostname_;
  base::WeakPtrFactory<QuicServerInfo> weak_factory_;
};

class QuicServerInfoFactory {
 public:
  virtual ~QuicServerInfoFactory();

  // GetForHost returns a fresh, allocated QuicServerInfo for the given
  // hostname or NULL on failure.
  virtual QuicServerInfo* GetForHost(const std::string& hostname) = 0;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_QUIC_SERVER_INFO_H_
