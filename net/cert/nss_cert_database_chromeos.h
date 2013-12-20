// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NSS_CERT_DATABASE_CHROMEOS_
#define NET_CERT_NSS_CERT_DATABASE_CHROMEOS_

#include "base/callback.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_profile_filter_chromeos.h"

namespace net {

class NET_EXPORT NSSCertDatabaseChromeOS : public NSSCertDatabase {
 public:
  NSSCertDatabaseChromeOS(crypto::ScopedPK11Slot public_slot,
                          crypto::ScopedPK11Slot private_slot);
  virtual ~NSSCertDatabaseChromeOS();

  // NSSCertDatabase implementation.
  virtual void ListCerts(CertificateList* certs) OVERRIDE;
  virtual crypto::ScopedPK11Slot GetPublicSlot() const OVERRIDE;
  virtual crypto::ScopedPK11Slot GetPrivateSlot() const OVERRIDE;
  virtual void ListModules(CryptoModuleList* modules, bool need_rw) const
      OVERRIDE;

  // TODO(mattm): handle trust setting, deletion, etc correctly when certs exist
  // in multiple slots.
  // TODO(mattm): handle trust setting correctly for certs in read-only slots.

 private:
  crypto::ScopedPK11Slot public_slot_;
  crypto::ScopedPK11Slot private_slot_;
  NSSProfileFilterChromeOS profile_filter_;

  DISALLOW_COPY_AND_ASSIGN(NSSCertDatabaseChromeOS);
};

}  // namespace net

#endif  // NET_CERT_NSS_CERT_DATABASE_CHROMEOS_
