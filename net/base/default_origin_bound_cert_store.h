// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DEFAULT_ORIGIN_BOUND_CERT_STORE_H_
#define NET_BASE_DEFAULT_ORIGIN_BOUND_CERT_STORE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "net/base/net_export.h"
#include "net/base/origin_bound_cert_store.h"

class Task;

namespace net {

// This class is the system for storing and retrieving origin bound certs.
// Modelled after the CookieMonster class, it has an in-memory cert store,
// and synchronizes origin bound certs to an optional permanent storage that
// implements the PersistentStore interface. The use case is described in
// http://balfanz.github.com/tls-obc-spec/draft-balfanz-tls-obc-00.html
//
// This class can be accessed by multiple threads. For example, it can be used
// by IO and origin bound cert management UI.
class NET_EXPORT DefaultOriginBoundCertStore : public OriginBoundCertStore {
 public:
  class OriginBoundCert;
  class PersistentStore;

  // The key for each OriginBoundCert* in OriginBoundCertMap is the
  // corresponding origin.
  typedef std::map<std::string, OriginBoundCert*> OriginBoundCertMap;

  // The store passed in should not have had Init() called on it yet. This
  // class will take care of initializing it. The backing store is NOT owned by
  // this class, but it must remain valid for the duration of the
  // DefaultOriginBoundCertStore's existence. If |store| is NULL, then no
  // backing store will be updated.
  explicit DefaultOriginBoundCertStore(PersistentStore* store);

  virtual ~DefaultOriginBoundCertStore();

  // Flush the backing store (if any) to disk and post the given task when done.
  // WARNING: THE CALLBACK WILL RUN ON A RANDOM THREAD. IT MUST BE THREAD SAFE.
  // It may be posted to the current thread, or it may run on the thread that
  // actually does the flushing. Your Task should generally post a notification
  // to the thread you actually want to be notified on.
  void FlushStore(const base::Closure& completion_task);

  // OriginBoundCertStore implementation.
  virtual bool GetOriginBoundCert(const std::string& origin,
                          std::string* private_key_result,
                          std::string* cert_result) OVERRIDE;
  virtual void SetOriginBoundCert(const std::string& origin,
                          const std::string& private_key,
                          const std::string& cert) OVERRIDE;
  virtual void DeleteOriginBoundCert(const std::string& origin) OVERRIDE;
  virtual void DeleteAll() OVERRIDE;
  virtual void GetAllOriginBoundCerts(
      std::vector<OriginBoundCertInfo>* origin_bound_certs) OVERRIDE;
  virtual int GetCertCount() OVERRIDE;

 private:
  static const size_t kMaxCerts;

  // Deletes all of the certs. Does not delete them from |store_|.
  void DeleteAllInMemory();

  // Called by all non-static functions to ensure that the cert store has
  // been initialized. This is not done during creating so it doesn't block
  // the window showing.
  // Note: this method should always be called with lock_ held.
  void InitIfNecessary() {
    if (!initialized_) {
      if (store_)
        InitStore();
      initialized_ = true;
    }
  }

  // Initializes the backing store and reads existing certs from it.
  // Should only be called by InitIfNecessary().
  void InitStore();

  // Deletes the cert for the specified origin, if such a cert exists, from the
  // in-memory store. Deletes it from |store_| if |store_| is not NULL.
  void InternalDeleteOriginBoundCert(const std::string& origin);

  // Takes ownership of *cert.
  // Adds the cert for the specified origin to the in-memory store. Deletes it
  // from |store_| if |store_| is not NULL.
  void InternalInsertOriginBoundCert(const std::string& origin,
                                     OriginBoundCert* cert);

  // Indicates whether the cert store has been initialized. This happens
  // Lazily in InitStoreIfNecessary().
  bool initialized_;

  scoped_refptr<PersistentStore> store_;

  OriginBoundCertMap origin_bound_certs_;

  // Lock for thread-safety
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(DefaultOriginBoundCertStore);
};

// The OriginBoundCert class contains a private key in addition to the origin
// and the cert.
class NET_EXPORT DefaultOriginBoundCertStore::OriginBoundCert {
 public:
  OriginBoundCert();
  OriginBoundCert(const std::string& origin,
                  const std::string& privatekey,
                  const std::string& cert);

  const std::string& origin() const { return origin_; }
  const std::string& private_key() const { return private_key_; }
  const std::string& cert() const { return cert_; }

 private:
  std::string origin_;
  std::string private_key_;
  std::string cert_;
};

typedef base::RefCountedThreadSafe<DefaultOriginBoundCertStore::PersistentStore>
    RefcountedPersistentStore;

class NET_EXPORT DefaultOriginBoundCertStore::PersistentStore
    : public RefcountedPersistentStore {
 public:
  virtual ~PersistentStore() {}

  // Initializes the store and retrieves the existing certs. This will be
  // called only once at startup. Note that the certs are individually allocated
  // and that ownership is transferred to the caller upon return.
  virtual bool Load(
      std::vector<DefaultOriginBoundCertStore::OriginBoundCert*>* certs) = 0;

  virtual void AddOriginBoundCert(const OriginBoundCert& cert) = 0;

  virtual void DeleteOriginBoundCert(const OriginBoundCert& cert) = 0;

  // Sets the value of the user preference whether the persistent storage
  // must be deleted upon destruction.
  virtual void SetClearLocalStateOnExit(bool clear_local_state) = 0;

  // Flush the store and post the given Task when complete.
  virtual void Flush(const base::Closure& completion_task) = 0;

 protected:
  PersistentStore();

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentStore);
};

}  // namespace net

#endif  // NET_DEFAULT_ORIGIN_BOUND_CERT_STORE_H_
