// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/cert_store_impl.h"

namespace web {

// static
CertStore* CertStore::GetInstance() {
  return CertStoreImpl::GetInstance();
}

//  static
CertStoreImpl* CertStoreImpl::GetInstance() {
  return base::Singleton<CertStoreImpl>::get();
}

CertStoreImpl::CertStoreImpl() {
}

CertStoreImpl::~CertStoreImpl() {
}

int CertStoreImpl::StoreCert(net::X509Certificate* cert, int group_id) {
  return store_.Store(cert, group_id);
}

bool CertStoreImpl::RetrieveCert(int cert_id,
                                 scoped_refptr<net::X509Certificate>* cert) {
  return store_.Retrieve(cert_id, cert);
}

void CertStoreImpl::RemoveCertsForGroup(int group_id) {
  store_.RemoveForRequestTracker(group_id);
}

}  // namespace web
