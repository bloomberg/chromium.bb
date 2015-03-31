// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSER_CERT_STORE_H_
#define IOS_WEB_PUBLIC_BROWSER_CERT_STORE_H_

#include "base/memory/ref_counted.h"

namespace net {
class X509Certificate;
}

namespace web {

// The purpose of the cert store is to provide an easy way to store/retrieve
// X509Certificate objects. When stored, an X509Certificate object is
// associated with a group, which can be cleared when it is no longer useful.
// It can be accessed from the UI and IO threads (it is thread-safe).
// Note that the cert ids will overflow if more than 2^32 - 1 certs are
// registered in one browsing session (which is highly unlikely to happen).
class CertStore {
 public:
  // Returns the singleton instance of the CertStore.
  static CertStore* GetInstance();

  // Stores the specified cert and returns the id associated with it.  The cert
  // is associated to the specified RequestTracker.
  // When all the RequestTrackers associated with a cert have been closed, the
  // cert is removed from the store.
  // Note: ids starts at 1.
  virtual int StoreCert(net::X509Certificate* cert, int group_id) = 0;

  // Tries to retrieve the previously stored cert associated with the specified
  // |cert_id|. Returns whether the cert could be found, and, if |cert| is
  // non-nullptr, copies it in.
  virtual bool RetrieveCert(int cert_id,
                            scoped_refptr<net::X509Certificate>* cert) = 0;

  // Removes all the certs associated with the specified groups from the store.
  virtual void RemoveCertsForGroup(int group_id) = 0;

 protected:
  virtual ~CertStore() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSER_CERT_STORE_H_
