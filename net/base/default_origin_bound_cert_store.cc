// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/default_origin_bound_cert_store.h"

#include "base/bind.h"
#include "base/message_loop.h"

namespace net {

// static
const size_t DefaultOriginBoundCertStore::kMaxCerts = 3300;

DefaultOriginBoundCertStore::DefaultOriginBoundCertStore(
    PersistentStore* store)
    : initialized_(false),
      store_(store) {}

void DefaultOriginBoundCertStore::FlushStore(
    const base::Closure& completion_task) {
  base::AutoLock autolock(lock_);

  if (initialized_ && store_)
    store_->Flush(completion_task);
  else if (!completion_task.is_null())
    MessageLoop::current()->PostTask(FROM_HERE, completion_task);
}

bool DefaultOriginBoundCertStore::GetOriginBoundCert(
    const std::string& origin,
    SSLClientCertType* type,
    std::string* private_key_result,
    std::string* cert_result) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  OriginBoundCertMap::iterator it = origin_bound_certs_.find(origin);

  if (it == origin_bound_certs_.end())
    return false;

  OriginBoundCert* cert = it->second;
  *type = cert->type();
  *private_key_result = cert->private_key();
  *cert_result = cert->cert();

  return true;
}

void DefaultOriginBoundCertStore::SetOriginBoundCert(
    const std::string& origin,
    SSLClientCertType type,
    const std::string& private_key,
    const std::string& cert) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  InternalDeleteOriginBoundCert(origin);
  InternalInsertOriginBoundCert(
      origin, new OriginBoundCert(origin, type, private_key, cert));
}

void DefaultOriginBoundCertStore::DeleteOriginBoundCert(
    const std::string& origin) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  InternalDeleteOriginBoundCert(origin);
}

void DefaultOriginBoundCertStore::DeleteAll() {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  for (OriginBoundCertMap::iterator it = origin_bound_certs_.begin();
       it != origin_bound_certs_.end(); ++it) {
    OriginBoundCert* cert = it->second;
    if (store_)
      store_->DeleteOriginBoundCert(*cert);
    delete cert;
  }
  origin_bound_certs_.clear();
}

void DefaultOriginBoundCertStore::GetAllOriginBoundCerts(
    std::vector<OriginBoundCert>* origin_bound_certs) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  for (OriginBoundCertMap::iterator it = origin_bound_certs_.begin();
       it != origin_bound_certs_.end(); ++it) {
    origin_bound_certs->push_back(*it->second);
  }
}

int DefaultOriginBoundCertStore::GetCertCount() {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  return origin_bound_certs_.size();
}

DefaultOriginBoundCertStore::~DefaultOriginBoundCertStore() {
  DeleteAllInMemory();
}

void DefaultOriginBoundCertStore::DeleteAllInMemory() {
  base::AutoLock autolock(lock_);

  for (OriginBoundCertMap::iterator it = origin_bound_certs_.begin();
       it != origin_bound_certs_.end(); ++it) {
    delete it->second;
  }
  origin_bound_certs_.clear();
}

void DefaultOriginBoundCertStore::InitStore() {
  lock_.AssertAcquired();

  DCHECK(store_) << "Store must exist to initialize";

  // Initialize the store and sync in any saved persistent certs.
  std::vector<OriginBoundCert*> certs;
  // Reserve space for the maximum amount of certs a database should have.
  // This prevents multiple vector growth / copies as we append certs.
  certs.reserve(kMaxCerts);
  store_->Load(&certs);

  for (std::vector<OriginBoundCert*>::const_iterator it = certs.begin();
       it != certs.end(); ++it) {
    origin_bound_certs_[(*it)->origin()] = *it;
  }
}

void DefaultOriginBoundCertStore::InternalDeleteOriginBoundCert(
    const std::string& origin) {
  lock_.AssertAcquired();

  OriginBoundCertMap::iterator it = origin_bound_certs_.find(origin);
  if (it == origin_bound_certs_.end())
    return;  // There is nothing to delete.

  OriginBoundCert* cert = it->second;
  if (store_)
    store_->DeleteOriginBoundCert(*cert);
  origin_bound_certs_.erase(it);
  delete cert;
}

void DefaultOriginBoundCertStore::InternalInsertOriginBoundCert(
    const std::string& origin,
    OriginBoundCert* cert) {
  lock_.AssertAcquired();

  if (store_)
    store_->AddOriginBoundCert(*cert);
  origin_bound_certs_[origin] = cert;
}

DefaultOriginBoundCertStore::PersistentStore::PersistentStore() {}

}  // namespace net
