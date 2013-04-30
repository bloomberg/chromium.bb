// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include <Security/Security.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

typedef OSStatus (*SecTrustSetAnchorCertificatesOnlyFuncPtr)(SecTrustRef,
                                                             Boolean);

Boolean OurSecCertificateEqual(const void* value1, const void* value2) {
  if (CFGetTypeID(value1) != SecCertificateGetTypeID() ||
      CFGetTypeID(value2) != SecCertificateGetTypeID())
    return CFEqual(value1, value2);
  return X509Certificate::IsSameOSCert(
      reinterpret_cast<SecCertificateRef>(const_cast<void*>(value1)),
      reinterpret_cast<SecCertificateRef>(const_cast<void*>(value2)));
}

const void* RetainWrapper(CFAllocatorRef unused, const void* value) {
  return CFRetain(value);
}

void ReleaseWrapper(CFAllocatorRef unused, const void* value) {
  CFRelease(value);
}

// CFEqual prior to 10.6 only performed pointer checks on SecCertificateRefs,
// rather than checking if they were the same (logical) certificate, so a
// custom structure is used for the array callbacks.
const CFArrayCallBacks kCertArrayCallbacks = {
  0,  // version
  RetainWrapper,
  ReleaseWrapper,
  CFCopyDescription,
  OurSecCertificateEqual,
};

}  // namespace

bool TestRootCerts::Add(X509Certificate* certificate) {
  if (CFArrayContainsValue(temporary_roots_,
                           CFRangeMake(0, CFArrayGetCount(temporary_roots_)),
                           certificate->os_cert_handle()))
    return true;
  CFArrayAppendValue(temporary_roots_, certificate->os_cert_handle());
  return true;
}

void TestRootCerts::Clear() {
  CFArrayRemoveAllValues(temporary_roots_);
}

bool TestRootCerts::IsEmpty() const {
  return CFArrayGetCount(temporary_roots_) == 0;
}

OSStatus TestRootCerts::FixupSecTrustRef(SecTrustRef trust_ref) const {
  if (IsEmpty())
    return noErr;

  // Despite SecTrustSetAnchorCertificatesOnly existing in OS X 10.6, and
  // being documented as available, it is not actually implemented. On 10.7+,
  // however, it always works.
  if (base::mac::IsOSLionOrLater()) {
    OSStatus status = SecTrustSetAnchorCertificates(trust_ref,
                                                    temporary_roots_);
    if (status)
      return status;
    // Trust system store in addition to trusting |temporary_roots_|.
    return SecTrustSetAnchorCertificatesOnly(trust_ref, false);
  }

  // For OS X 10.6, emulate the functionality by copying the system roots
  // in addition to |temporary_roots_|.
  CFArrayRef system_roots = NULL;
  OSStatus status = SecTrustCopyAnchorCertificates(&system_roots);
  if (status)
    return status;

  base::mac::ScopedCFTypeRef<CFArrayRef> scoped_system_roots(system_roots);
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> scoped_roots(
      CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, scoped_system_roots));
  CFArrayAppendArray(scoped_roots, temporary_roots_,
                     CFRangeMake(0, CFArrayGetCount(temporary_roots_)));
  return SecTrustSetAnchorCertificates(trust_ref, scoped_roots);
}

TestRootCerts::~TestRootCerts() {}

void TestRootCerts::Init() {
  temporary_roots_.reset(CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                              &kCertArrayCallbacks));
}

}  // namespace net
