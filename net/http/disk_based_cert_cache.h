// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_DISK_BASED_CERT_CACHE_H
#define NET_HTTP_DISK_BASED_CERT_CACHE_H

#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace disk_cache {
class Backend;
}  // namespace disk_cache

namespace net {

// DiskBasedCertCache is used to store and retrieve X.509 certificates from the
// cache. Each individual certificate is stored separately from its certificate
// chain. No more than one copy (per certificate) will be stored on disk.
class NET_EXPORT_PRIVATE DiskBasedCertCache {
 public:
  typedef base::Callback<void(const X509Certificate::OSCertHandle cert_handle)>
      GetCallback;
  typedef base::Callback<void(const std::string&)> SetCallback;

  // Initializes a new DiskBasedCertCache that will use |backend|, which has
  // previously been initialized, to store the certificate in the cache.
  explicit DiskBasedCertCache(disk_cache::Backend* backend);
  ~DiskBasedCertCache();

  // Fetches the certificate associated with |key|. If the certificate is
  // found within the cache, |cb| will be called with the certificate.
  // Otherwise, |cb| will be called with NULL. Callers that wish to store
  // a reference to the certificate need to use X509Certificate::DupOSCertHandle
  // inside |cb|.
  void Get(const std::string& key, const GetCallback& cb);

  // Stores |cert_handle| in the cache. If |cert_handle| is successfully stored,
  // |cb| will be called with the key. If |cb| is called with an empty
  // string, then |cert_handle| was not stored.
  void Set(const X509Certificate::OSCertHandle cert_handle,
           const SetCallback& cb);

  // Returns the number of in-memory MRU cache hits that have occurred
  // on Set and Get operations. Intended for test purposes only.
  size_t mem_cache_hits_for_testing() const { return mem_cache_hits_; }

  // Returns the number of in-memory MRU cache misses that have occurred
  // on Set and Get operations. Intended for test purposes only.
  size_t mem_cache_misses_for_testing() const { return mem_cache_misses_; }

 private:
  class ReadWorker;
  class WriteWorker;

  // A functor used to free an OSCertHandle. Used by the MRUCertCache.
  struct CertFree {
    void operator()(X509Certificate::OSCertHandle cert_handle);
  };

  // An in-memory cache that is used to prevent redundant reads and writes
  // to and from the disk cache.
  typedef base::MRUCacheBase<std::string,
                             X509Certificate::OSCertHandle,
                             CertFree> MRUCertCache;

  // ReadWorkerMap and WriteWorkerMap map cache keys to their
  // corresponding Workers.
  typedef base::hash_map<std::string, ReadWorker*> ReadWorkerMap;
  typedef base::hash_map<std::string, WriteWorker*> WriteWorkerMap;

  // FinishedReadOperation and FinishedWriteOperation are used by callbacks
  // given to the workers to signal the DiskBasedCertCache they have completed
  // their work.
  void FinishedReadOperation(const std::string& key,
                             X509Certificate::OSCertHandle cert_handle);
  void FinishedWriteOperation(const std::string& key,
                              X509Certificate::OSCertHandle cert_handle);

  disk_cache::Backend* backend_;

  ReadWorkerMap read_worker_map_;
  WriteWorkerMap write_worker_map_;
  MRUCertCache mru_cert_cache_;

  int mem_cache_hits_;
  int mem_cache_misses_;

  base::WeakPtrFactory<DiskBasedCertCache> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(DiskBasedCertCache);
};

}  // namespace net

#endif  // NET_HTTP_DISK_BASED_CERT_CACHE_H
