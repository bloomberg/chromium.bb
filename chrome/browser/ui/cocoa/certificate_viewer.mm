// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/certificate_viewer.h"

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

  [[[SFCertificatePanel alloc] init]
      beginSheetForWindow:parent
            modalDelegate:nil
           didEndSelector:NULL
              contextInfo:NULL
             certificates:reinterpret_cast<NSArray*>(certificates.get())
                showGroup:YES];
  // The SFCertificatePanel releases itself when the sheet is dismissed.
}
