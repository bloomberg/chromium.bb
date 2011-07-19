// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include <Security/Security.h>
#include <SecurityInterface/SFCertificatePanel.h>

#include <vector>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "net/base/x509_certificate.h"

void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  SecCertificateRef cert_mac = cert->os_cert_handle();
  if (!cert_mac)
    return;

  base::mac::ScopedCFTypeRef<CFMutableArrayRef> certificates(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!certificates.get()) {
    NOTREACHED();
    return;
  }
  CFArrayAppendValue(certificates, cert_mac);

  // Server certificate must be first in the array; subsequent certificates
  // in the chain can be in any order.
  const std::vector<SecCertificateRef>& ca_certs =
      cert->GetIntermediateCertificates();
  for (size_t i = 0; i < ca_certs.size(); ++i)
    CFArrayAppendValue(certificates, ca_certs[i]);

  // Explicitly disable revocation checking, regardless of user preferences
  // or system settings. The behaviour of SFCertificatePanel is to call
  // SecTrustEvaluate on the certificate(s) supplied, effectively
  // duplicating the behaviour of net::X509Certificate::Verify(). However,
  // this call stalls the UI if revocation checking is enabled in the
  // Keychain preferences or if the cert may be an EV cert. By disabling
  // revocation checking, the stall is limited to the time taken for path
  // building and verification, which should be minimized due to the path
  // being provided in |certificates|. This does not affect normal
  // revocation checking from happening, which is controlled by
  // net::X509Certificate::Verify() and user preferences, but will prevent
  // the certificate viewer UI from displaying which certificate is revoked.
  // This is acceptable, as certificate revocation will still be shown in
  // the page info bubble if a certificate in the chain is actually revoked.
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!policies.get()) {
    NOTREACHED();
    return;
  }
  // Add a basic X.509 policy, in order to match the behaviour of
  // SFCertificatePanel when no policies are specified.
  SecPolicyRef basic_policy = NULL;
  OSStatus status = net::X509Certificate::CreateBasicX509Policy(&basic_policy);
  if (status != noErr) {
    NOTREACHED();
    return;
  }
  CFArrayAppendValue(policies, basic_policy);
  CFRelease(basic_policy);

  status = net::X509Certificate::CreateRevocationPolicies(false, policies);
  if (status != noErr) {
    NOTREACHED();
    return;
  }

  SFCertificatePanel* panel = [[SFCertificatePanel alloc] init];
  [panel setPolicies:(id)policies.get()];
  [panel beginSheetForWindow:parent
               modalDelegate:nil
              didEndSelector:NULL
                 contextInfo:NULL
                certificates:reinterpret_cast<NSArray*>(certificates.get())
                   showGroup:YES];
  // The SFCertificatePanel releases itself when the sheet is dismissed.
}
