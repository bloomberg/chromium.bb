// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_ios.h"

#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace x509_util {

base::ScopedCFTypeRef<SecCertificateRef> CreateSecCertificateFromBytes(
    const uint8_t* data,
    size_t length) {
  base::ScopedCFTypeRef<CFDataRef> cert_data(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data),
      base::checked_cast<CFIndex>(length), kCFAllocatorNull));
  if (!cert_data)
    return base::ScopedCFTypeRef<SecCertificateRef>();

  return base::ScopedCFTypeRef<SecCertificateRef>(
      SecCertificateCreateWithData(nullptr, cert_data));
}

base::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromX509Certificate(const X509Certificate* cert) {
#if BUILDFLAG(USE_BYTE_CERTS)
  return CreateSecCertificateFromBytes(
      CRYPTO_BUFFER_data(cert->os_cert_handle()),
      CRYPTO_BUFFER_len(cert->os_cert_handle()));
#else
  return base::ScopedCFTypeRef<SecCertificateRef>(
      reinterpret_cast<SecCertificateRef>(
          const_cast<void*>(CFRetain(cert->os_cert_handle()))));
#endif
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    SecCertificateRef sec_cert,
    const std::vector<SecCertificateRef>& sec_chain) {
#if BUILDFLAG(USE_BYTE_CERTS)
  if (!sec_cert)
    return nullptr;
  base::ScopedCFTypeRef<CFDataRef> der_data(SecCertificateCopyData(sec_cert));
  if (!der_data)
    return nullptr;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(
      X509Certificate::CreateOSCertHandleFromBytes(
          reinterpret_cast<const char*>(CFDataGetBytePtr(der_data)),
          CFDataGetLength(der_data)));
  if (!cert_handle)
    return nullptr;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  X509Certificate::OSCertHandles intermediates_raw;
  for (const SecCertificateRef& sec_intermediate : sec_chain) {
    if (!sec_intermediate)
      return nullptr;
    der_data.reset(SecCertificateCopyData(sec_intermediate));
    if (!der_data)
      return nullptr;
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle(
        X509Certificate::CreateOSCertHandleFromBytes(
            reinterpret_cast<const char*>(CFDataGetBytePtr(der_data)),
            CFDataGetLength(der_data)));
    if (!intermediate_cert_handle)
      return nullptr;
    intermediates_raw.push_back(intermediate_cert_handle.get());
    intermediates.push_back(std::move(intermediate_cert_handle));
  }
  scoped_refptr<X509Certificate> result(
      X509Certificate::CreateFromHandle(cert_handle.get(), intermediates_raw));
  return result;
#else
  return X509Certificate::CreateFromHandle(sec_cert, sec_chain);
#endif
}

}  // namespace x509_util

}  // namespace net
