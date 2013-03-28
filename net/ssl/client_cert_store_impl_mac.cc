// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_impl.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFArray.h>
#include <CoreServices/CoreServices.h>
#include <Security/SecBase.h>
#include <Security/Security.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "net/base/host_port_pair.h"
#include "net/base/x509_util.h"

using base::mac::ScopedCFTypeRef;

namespace net {

namespace {

bool GetClientCertsImpl(const scoped_refptr<X509Certificate>& preferred_cert,
                        const CertificateList& regular_certs,
                        const SSLCertRequestInfo& request,
                        CertificateList* selected_certs) {
  CertificateList preliminary_list;
  if (preferred_cert)
    preliminary_list.push_back(preferred_cert);
  preliminary_list.insert(preliminary_list.end(), regular_certs.begin(),
                          regular_certs.end());

  selected_certs->clear();
  for (size_t i = 0; i < preliminary_list.size(); ++i) {
    scoped_refptr<X509Certificate>& cert = preliminary_list[i];
    if (cert->HasExpired() || !cert->SupportsSSLClientAuth())
      continue;

    // Skip duplicates (a cert may be in multiple keychains).
    const SHA1HashValue& fingerprint = cert->fingerprint();
    size_t pos;
    for (pos = 0; pos < selected_certs->size(); ++pos) {
      if ((*selected_certs)[pos]->fingerprint().Equals(fingerprint))
        break;
    }
    if (pos < selected_certs->size())
      continue;

    // Check if the certificate issuer is allowed by the server.
    if (!request.cert_authorities.empty() &&
        !cert->IsIssuedByEncoded(request.cert_authorities)) {
      continue;
    }
    selected_certs->push_back(cert);
  }

  // Preferred cert should appear first in the ui, so exclude it from the
  // sorting.
  CertificateList::iterator sort_begin = selected_certs->begin();
  CertificateList::iterator sort_end = selected_certs->end();
  if (preferred_cert &&
      sort_begin != sort_end &&
      *sort_begin == preferred_cert) {
    ++sort_begin;
  }
  sort(sort_begin, sort_end, x509_util::ClientCertSorter());
  return true;
}

}  // namespace

bool ClientCertStoreImpl::GetClientCerts(const SSLCertRequestInfo& request,
                                         CertificateList* selected_certs) {
  std::string server_domain =
      HostPortPair::FromString(request.host_and_port).host();

  ScopedCFTypeRef<SecIdentityRef> preferred_identity;
  if (!server_domain.empty()) {
    // See if there's an identity preference for this domain:
    ScopedCFTypeRef<CFStringRef> domain_str(
        base::SysUTF8ToCFStringRef("https://" + server_domain));
    SecIdentityRef identity = NULL;
    // While SecIdentityCopyPreferences appears to take a list of CA issuers
    // to restrict the identity search to, within Security.framework the
    // argument is ignored and filtering unimplemented. See
    // SecIdentity.cpp in libsecurity_keychain, specifically
    // _SecIdentityCopyPreferenceMatchingName().
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      if (SecIdentityCopyPreference(domain_str, 0, NULL, &identity) == noErr)
        preferred_identity.reset(identity);
    }
  }

  // Now enumerate the identities in the available keychains.
  scoped_refptr<X509Certificate> preferred_cert = NULL;
  CertificateList regular_certs;

  SecIdentitySearchRef search = NULL;
  OSStatus err;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    err = SecIdentitySearchCreate(NULL, CSSM_KEYUSE_SIGN, &search);
  }
  if (err)
    return false;
  ScopedCFTypeRef<SecIdentitySearchRef> scoped_search(search);
  while (!err) {
    SecIdentityRef identity = NULL;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      err = SecIdentitySearchCopyNext(search, &identity);
    }
    if (err)
      break;
    ScopedCFTypeRef<SecIdentityRef> scoped_identity(identity);

    SecCertificateRef cert_handle;
    err = SecIdentityCopyCertificate(identity, &cert_handle);
    if (err != noErr)
      continue;
    ScopedCFTypeRef<SecCertificateRef> scoped_cert_handle(cert_handle);

    scoped_refptr<X509Certificate> cert(
        X509Certificate::CreateFromHandle(cert_handle,
                                          X509Certificate::OSCertHandles()));

    if (preferred_identity && CFEqual(preferred_identity, identity)) {
      // Only one certificate should match.
      DCHECK(!preferred_cert);
      preferred_cert = cert;
    } else {
      regular_certs.push_back(cert);
    }
  }

  if (err != errSecItemNotFound) {
    OSSTATUS_LOG(ERROR, err) << "SecIdentitySearch error";
    return false;
  }

  return GetClientCertsImpl(preferred_cert, regular_certs, request,
                            selected_certs);
}

bool ClientCertStoreImpl::SelectClientCerts(const CertificateList& input_certs,
                                            const SSLCertRequestInfo& request,
                                            CertificateList* selected_certs) {
  return GetClientCertsImpl(NULL, input_certs, request,
                            selected_certs);
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool ClientCertStoreImpl::SelectClientCertsGivenPreferred(
      const scoped_refptr<X509Certificate>& preferred_cert,
      const CertificateList& regular_certs,
      const SSLCertRequestInfo& request,
      CertificateList* selected_certs) {
  return GetClientCertsImpl(preferred_cert, regular_certs, request,
                            selected_certs);
}
#endif

}  // namespace net
