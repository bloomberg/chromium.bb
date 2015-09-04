// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_THREAD_SAFE_CERTIFICATE_MAP_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_THREAD_SAFE_CERTIFICATE_MAP_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_info.h"

namespace net {
class SHA256HashValueLessThan;
class X509Certificate;
struct SHA256HashValue;
}

namespace chromeos {
namespace certificate_provider {

class ThreadSafeCertificateMap {
 public:
  using FingerprintToCertMap = std::map<net::SHA256HashValue,
                                        CertificateInfo,
                                        net::SHA256HashValueLessThan>;
  using ExtensionToFingerprintsMap =
      std::map<std::string, FingerprintToCertMap>;

  ThreadSafeCertificateMap();
  ~ThreadSafeCertificateMap();

  // Replaces the stored certificates by the given certificates.
  void Update(const std::map<std::string, CertificateInfoList>&
                  extension_to_certificates);

  // Looks up the given certificate. If found, it returns true and sets
  // |extension_id| to the extension id for which this certificate was
  // registered and sets |info| to the stored info. Otherwise returns false and
  // doesn't modify |info| and |extension_id|.
  bool LookUpCertificate(const net::X509Certificate& cert,
                         CertificateInfo* info,
                         std::string* extension_id);

  // Remove all certificates that are currently registered for the given
  // extension.
  void RemoveExtension(const std::string& extension_id);

 private:
  base::Lock lock_;
  ExtensionToFingerprintsMap extension_to_certificates_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeCertificateMap);
};

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_THREAD_SAFE_CERTIFICATE_MAP_H_
