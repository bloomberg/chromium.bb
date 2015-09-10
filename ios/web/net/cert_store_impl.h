// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CERT_STORE_IMPL_H_
#define IOS_WEB_NET_CERT_STORE_IMPL_H_

#include "base/memory/singleton.h"
#include "ios/web/net/request_tracker_data_memoizing_store.h"
#include "ios/web/public/cert_store.h"
#include "net/cert/x509_certificate.h"

namespace web {

class CertStoreImpl : public CertStore {
 public:
  // Returns the singleton instance of the CertStore.
  static CertStoreImpl* GetInstance();

  // CertStore implementation:
  int StoreCert(net::X509Certificate* cert, int group_id) override;
  bool RetrieveCert(int cert_id,
                    scoped_refptr<net::X509Certificate>* cert) override;
  void RemoveCertsForGroup(int group_id) override;

 protected:
  CertStoreImpl();
  ~CertStoreImpl() override;

 private:
  friend struct base::DefaultSingletonTraits<CertStoreImpl>;

  RequestTrackerDataMemoizingStore<net::X509Certificate> store_;

  DISALLOW_COPY_AND_ASSIGN(CertStoreImpl);
};

}  // namespace web

#endif  // IOS_WEB_NET_CERT_STORE_IMPL_H_
