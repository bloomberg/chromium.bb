// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SQLITE_SERVER_BOUND_CERT_STORE_H_
#define CHROME_BROWSER_NET_SQLITE_SERVER_BOUND_CERT_STORE_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/ssl/default_server_bound_cert_store.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace quota {
class SpecialStoragePolicy;
}

// Implements the net::DefaultServerBoundCertStore::PersistentStore interface
// in terms of a SQLite database. For documentation about the actual member
// functions consult the documentation of the parent class
// |net::DefaultServerBoundCertStore::PersistentCertStore|.
// If provided, a |SpecialStoragePolicy| is consulted when the SQLite database
// is closed to decide which certificates to keep.
class SQLiteServerBoundCertStore
    : public net::DefaultServerBoundCertStore::PersistentStore {
 public:
  SQLiteServerBoundCertStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      quota::SpecialStoragePolicy* special_storage_policy);

  // net::DefaultServerBoundCertStore::PersistentStore:
  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE;
  virtual void AddServerBoundCert(
      const net::DefaultServerBoundCertStore::ServerBoundCert& cert) OVERRIDE;
  virtual void DeleteServerBoundCert(
      const net::DefaultServerBoundCertStore::ServerBoundCert& cert) OVERRIDE;
  virtual void SetForceKeepSessionState() OVERRIDE;

 protected:
  virtual ~SQLiteServerBoundCertStore();

 private:
  class Backend;

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(SQLiteServerBoundCertStore);
};

#endif  // CHROME_BROWSER_NET_SQLITE_SERVER_BOUND_CERT_STORE_H_
