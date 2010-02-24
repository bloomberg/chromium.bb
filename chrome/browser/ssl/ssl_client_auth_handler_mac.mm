// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_handler.h"

#import <SecurityInterface/SFChooseIdentityPanel.h>

#include "app/l10n_util_mac.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"

void SSLClientAuthHandler::DoSelectCertificate() {
  net::X509Certificate* cert = NULL;
  // Create an array of CFIdentityRefs for the certificates:
  size_t num_certs = cert_request_info_->client_certs.size();
  NSMutableArray* identities = [NSMutableArray arrayWithCapacity:num_certs];
  for (size_t i = 0; i < num_certs; ++i) {
    SecCertificateRef cert;
    cert = cert_request_info_->client_certs[i]->os_cert_handle();
    SecIdentityRef identity;
    if (SecIdentityCreateWithCertificate(NULL, cert, &identity) == noErr) {
      [identities addObject:(id)identity];
      CFRelease(identity);
    }
  }

  // Get the message to display:
  NSString* title = l10n_util::GetNSString(IDS_CLIENT_CERT_DIALOG_TITLE);
  NSString* message = l10n_util::GetNSStringF(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      ASCIIToUTF16(cert_request_info_->host_and_port));

  // Create and set up a system choose-identity panel.
  scoped_nsobject<SFChooseIdentityPanel> panel (
      [[SFChooseIdentityPanel alloc] init]);
  NSString* domain = base::SysUTF8ToNSString(
      "https://" + cert_request_info_->host_and_port);
  [panel setDomain:domain];
  [panel setInformativeText:message];
  [panel setAlternateButtonTitle:l10n_util::GetNSString(IDS_CANCEL)];
  SecPolicyRef sslPolicy;
  if (net::X509Certificate::CreateSSLClientPolicy(&sslPolicy) == noErr) {
    [panel setPolicies:(id)sslPolicy];
    CFRelease(sslPolicy);
  }

  // Run the panel, modally.
  // TODO(snej): Change this into a sheet so it doesn't block the runloop!
  if ([panel runModalForIdentities:identities message:title] == NSOKButton) {
    NSUInteger index = [identities indexOfObject:(id)[panel identity]];
    DCHECK(index != NSNotFound);
    cert = cert_request_info_->client_certs[index];
  }

  // Finally, tell the back end which identity (or none) the user selected.
  CertificateSelected(cert);
}
