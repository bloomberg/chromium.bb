// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SERVER_BOUND_CERT_SERVICE_H_
#define NET_SSL_SERVER_BOUND_CERT_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/ssl/server_bound_cert_store.h"
#include "net/ssl/ssl_client_cert_type.h"

namespace base {
class TaskRunner;
}

namespace net {

class ServerBoundCertServiceJob;
class ServerBoundCertServiceRequest;
class ServerBoundCertServiceWorker;

// A class for creating and fetching server bound certs. These certs are used
// to identify users' machines; their public keys are used as channel IDs in
// http://tools.ietf.org/html/draft-balfanz-tls-channelid-00.
// As a result although certs are set to be invalid after one year, we don't
// actually expire them. Once generated, certs are valid as long as the users
// want. Users can delete existing certs, and new certs will be generated
// automatically.

// Inherits from NonThreadSafe in order to use the function
// |CalledOnValidThread|.
class NET_EXPORT ServerBoundCertService
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  class NET_EXPORT RequestHandle {
   public:
    RequestHandle();
    ~RequestHandle();

    // Cancel the request.  Does nothing if the request finished or was already
    // cancelled.
    void Cancel();

    bool is_active() const { return request_ != NULL; }

   private:
    friend class ServerBoundCertService;

    void RequestStarted(ServerBoundCertService* service,
                        ServerBoundCertServiceRequest* request,
                        const CompletionCallback& callback);

    void OnRequestComplete(int result);

    ServerBoundCertService* service_;
    ServerBoundCertServiceRequest* request_;
    CompletionCallback callback_;
  };

  // Password used on EncryptedPrivateKeyInfo data stored in EC private_key
  // values.  (This is not used to provide any security, but to workaround NSS
  // being unable to import unencrypted PrivateKeyInfo for EC keys.)
  static const char kEPKIPassword[];

  // This object owns |server_bound_cert_store|.  |task_runner| will
  // be used to post certificate generation worker tasks.  The tasks are
  // safe for use with WorkerPool and SequencedWorkerPool::CONTINUE_ON_SHUTDOWN.
  ServerBoundCertService(
      ServerBoundCertStore* server_bound_cert_store,
      const scoped_refptr<base::TaskRunner>& task_runner);

  ~ServerBoundCertService();

  // Returns the domain to be used for |host|.  The domain is the
  // "registry controlled domain", or the "ETLD + 1" where one exists, or
  // the origin otherwise.
  static std::string GetDomainForHost(const std::string& host);

  // Tests whether the system time is within the supported range for
  // certificate generation.  This value is cached when ServerBoundCertService
  // is created, so if the system time is changed by a huge amount, this may no
  // longer hold.
  bool IsSystemTimeValid() const { return is_system_time_valid_; }

  // Fetches the domain bound cert for the specified origin of the specified
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
  // |*out_req| will be initialized with a handle to the async request. This
  // RequestHandle object must be cancelled or destroyed before the
  // ServerBoundCertService is destroyed.
  int GetDomainBoundCert(
      const std::string& origin,
      const std::vector<uint8>& requested_types,
      SSLClientCertType* type,
      std::string* private_key,
      std::string* cert,
      const CompletionCallback& callback,
      RequestHandle* out_req);

  // Returns the backing ServerBoundCertStore.
  ServerBoundCertStore* GetCertStore();

  // Public only for unit testing.
  int cert_count();
  uint64 requests() const { return requests_; }
  uint64 cert_store_hits() const { return cert_store_hits_; }
  uint64 inflight_joins() const { return inflight_joins_; }

 private:
  // Cancels the specified request. |req| is the handle stored by
  // GetDomainBoundCert(). After a request is canceled, its completion
  // callback will not be called.
  void CancelRequest(ServerBoundCertServiceRequest* req);

  void GotServerBoundCert(const std::string& server_identifier,
                          SSLClientCertType type,
                          base::Time expiration_time,
                          const std::string& key,
                          const std::string& cert);
  void GeneratedServerBoundCert(
      const std::string& server_identifier,
      int error,
      scoped_ptr<ServerBoundCertStore::ServerBoundCert> cert);
  void HandleResult(int error,
                    const std::string& server_identifier,
                    SSLClientCertType type,
                    const std::string& private_key,
                    const std::string& cert);

  scoped_ptr<ServerBoundCertStore> server_bound_cert_store_;
  scoped_refptr<base::TaskRunner> task_runner_;

  // inflight_ maps from a server to an active generation which is taking
  // place.
  std::map<std::string, ServerBoundCertServiceJob*> inflight_;
  base::WeakPtrFactory<ServerBoundCertService> weak_ptr_factory_;

  uint64 requests_;
  uint64 cert_store_hits_;
  uint64 inflight_joins_;

  bool is_system_time_valid_;

  DISALLOW_COPY_AND_ASSIGN(ServerBoundCertService);
};

}  // namespace net

#endif  // NET_SSL_SERVER_BOUND_CERT_SERVICE_H_
