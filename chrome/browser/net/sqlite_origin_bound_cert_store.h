// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SQLITE_ORIGIN_BOUND_CERT_STORE_H_
#define CHROME_BROWSER_NET_SQLITE_ORIGIN_BOUND_CERT_STORE_H_
#pragma once

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/default_origin_bound_cert_store.h"

class FilePath;

// Implements the net::DefaultOriginBoundCertStore::PersistentStore interface
// in terms of a SQLite database. For documentation about the actual member
// functions consult the documentation of the parent class
// |net::DefaultOriginBoundCertStore::PersistentCertStore|.
class SQLiteOriginBoundCertStore
    : public net::DefaultOriginBoundCertStore::PersistentStore {
 public:
  explicit SQLiteOriginBoundCertStore(const FilePath& path);
  virtual ~SQLiteOriginBoundCertStore();

  // net::DefaultOriginBoundCertStore::PersistentStore implementation.
  virtual bool Load(
      std::vector<net::DefaultOriginBoundCertStore::OriginBoundCert*>* certs)
          OVERRIDE;
  virtual void AddOriginBoundCert(
      const net::DefaultOriginBoundCertStore::OriginBoundCert& cert) OVERRIDE;
  virtual void DeleteOriginBoundCert(
      const net::DefaultOriginBoundCertStore::OriginBoundCert& cert) OVERRIDE;
  virtual void SetClearLocalStateOnExit(bool clear_local_state) OVERRIDE;
  virtual void Flush(const base::Closure& completion_task) OVERRIDE;

 private:
  class Backend;

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(SQLiteOriginBoundCertStore);
};

#endif  // CHROME_BROWSER_NET_SQLITE_ORIGIN_BOUND_CERT_STORE_H_
