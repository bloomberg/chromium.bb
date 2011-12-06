// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
#define NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/ssl_client_cert_type.h"

namespace net {

class OriginBoundCertServiceJob;
class OriginBoundCertServiceWorker;
class OriginBoundCertStore;

// A class for creating and fetching origin bound certs.
// Inherits from NonThreadSafe in order to use the function
// |CalledOnValidThread|.
class NET_EXPORT OriginBoundCertService
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Opaque type used to cancel a request.
  typedef void* RequestHandle;

  // Password used on EncryptedPrivateKeyInfo data stored in EC private_key
  // values.  (This is not used to provide any security, but to workaround NSS
  // being unable to import unencrypted PrivateKeyInfo for EC keys.)
  static const char kEPKIPassword[];

  // This object owns origin_bound_cert_store.
  explicit OriginBoundCertService(
      OriginBoundCertStore* origin_bound_cert_store);

  ~OriginBoundCertService();

  // Fetches the origin bound cert for the specified origin of the specified
  // type if one exists and creates one otherwise. Returns OK if successful or
  // an error code upon failure.
  //
  // |requested_types| is a list of the TLS ClientCertificateTypes the site will
  // accept, ordered from most preferred to least preferred.  Types we don't
  // support will be ignored. See ssl_client_cert_type.h.
  //
  // On successful completion, |private_key| stores a DER-encoded
  // PrivateKeyInfo struct, and |cert| stores a DER-encoded certificate, and
  // |type| specifies the type of certificate that was returned.
  //
  // |callback| must not be null. ERR_IO_PENDING is returned if the operation
  // could not be completed immediately, in which case the result code will
  // be passed to the callback when available.
  //
  // If |out_req| is non-NULL, then |*out_req| will be filled with a handle to
  // the async request. This handle is not valid after the request has
  // completed.
  int GetOriginBoundCert(
      const std::string& origin,
      const std::vector<uint8>& requested_types,
      SSLClientCertType* type,
      std::string* private_key,
      std::string* cert,
      const CompletionCallback& callback,
      RequestHandle* out_req);

  // Cancels the specified request. |req| is the handle returned by
  // GetOriginBoundCert(). After a request is canceled, its completion
  // callback will not be called.
  void CancelRequest(RequestHandle req);

  // Public only for unit testing.
  int cert_count();
  uint64 requests() const { return requests_; }
  uint64 cert_store_hits() const { return cert_store_hits_; }
  uint64 inflight_joins() const { return inflight_joins_; }

 private:
  friend class OriginBoundCertServiceWorker;  // Calls HandleResult.

  // On success, |private_key| stores a DER-encoded PrivateKeyInfo
  // struct, and |cert| stores a DER-encoded certificate. Returns
  // OK if successful and an error code otherwise.
  // |serial_number| is passed in because it is created with the function
  // base::RandInt, which opens the file /dev/urandom. /dev/urandom is opened
  // with a LazyInstance, which is not allowed on a worker thread.
  static int GenerateCert(const std::string& origin,
                          SSLClientCertType type,
                          uint32 serial_number,
                          std::string* private_key,
                          std::string* cert);

  void HandleResult(const std::string& origin,
                    int error,
                    SSLClientCertType type,
                    const std::string& private_key,
                    const std::string& cert);

  scoped_ptr<OriginBoundCertStore> origin_bound_cert_store_;

  // inflight_ maps from an origin to an active generation which is taking
  // place.
  std::map<std::string, OriginBoundCertServiceJob*> inflight_;

  uint64 requests_;
  uint64 cert_store_hits_;
  uint64 inflight_joins_;

  DISALLOW_COPY_AND_ASSIGN(OriginBoundCertService);
};

}  // namespace net

#endif  // NET_BASE_ORIGIN_BOUND_CERT_SERVICE_H_
