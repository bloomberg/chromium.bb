// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_NSS_PROFILE_FILTER_CHROMEOS_H_
#define NET_CERT_NSS_PROFILE_FILTER_CHROMEOS_H_

#include "base/memory/scoped_ptr.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/crypto_module.h"
#include "net/base/net_export.h"

namespace net {

class X509Certificate;

class NET_EXPORT NSSProfileFilterChromeOS {
 public:
  NSSProfileFilterChromeOS();
  NSSProfileFilterChromeOS(const NSSProfileFilterChromeOS& other);
  ~NSSProfileFilterChromeOS();

  NSSProfileFilterChromeOS& operator=(const NSSProfileFilterChromeOS& other);

  // Initializes the filter with slot handles.
  void Init(crypto::ScopedPK11Slot public_slot,
            crypto::ScopedPK11Slot private_slot);

  bool IsModuleAllowed(PK11SlotInfo* slot) const;
  bool IsCertAllowed(const scoped_refptr<X509Certificate>& cert) const;

  class CertNotAllowedForProfilePredicate {
   public:
    explicit CertNotAllowedForProfilePredicate(
        const NSSProfileFilterChromeOS& filter);
    bool operator()(const scoped_refptr<X509Certificate>& cert) const;

   private:
    const NSSProfileFilterChromeOS& filter_;
  };

  class ModuleNotAllowedForProfilePredicate {
   public:
    explicit ModuleNotAllowedForProfilePredicate(
        const NSSProfileFilterChromeOS& filter);
    bool operator()(const scoped_refptr<CryptoModule>& module) const;

   private:
    const NSSProfileFilterChromeOS& filter_;
  };

 private:
  crypto::ScopedPK11Slot public_slot_;
  crypto::ScopedPK11Slot private_slot_;
};

}  // namespace net

#endif  // NET_CERT_NSS_PROFILE_FILTER_CHROMEOS_H_
