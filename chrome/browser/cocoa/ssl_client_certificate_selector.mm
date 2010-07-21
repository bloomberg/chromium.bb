// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl_client_certificate_selector.h"

#import <SecurityInterface/SFChooseIdentityPanel.h>

#include <vector>

#import "app/l10n_util_mac.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_cftyperef.h"
#import "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/ssl/ssl_client_auth_handler.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"

@interface SSLClientCertificateSelectorCocoa : NSObject {
 @private
  // The handler to report back to.
  scoped_refptr<SSLClientAuthHandler> handler_;
  // The certificate request we serve.
  scoped_refptr<net::SSLCertRequestInfo> certRequestInfo_;
  // The list of identities offered to the user.
  scoped_nsobject<NSMutableArray> identities_;
  // The corresponding list of certificates.
  std::vector<scoped_refptr<net::X509Certificate> > certificates_;
  // The panel we display.
  scoped_nsobject<SFChooseIdentityPanel> panel_;
}

- (id)initWithHandler:(SSLClientAuthHandler*)handler
      certRequestInfo:(net::SSLCertRequestInfo*)certRequestInfo;
- (void)displayDialog:(gfx::NativeWindow)parent;
@end

namespace browser {

void ShowSSLClientCertificateSelector(
    gfx::NativeWindow parent,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  SSLClientCertificateSelectorCocoa* selector =
      [[[SSLClientCertificateSelectorCocoa alloc]
          initWithHandler:delegate
          certRequestInfo:cert_request_info] autorelease];
  [selector displayDialog:parent];
}

}  // namespace browser

@implementation SSLClientCertificateSelectorCocoa

- (id)initWithHandler:(SSLClientAuthHandler*)handler
      certRequestInfo:(net::SSLCertRequestInfo*)certRequestInfo {
  DCHECK(handler);
  DCHECK(certRequestInfo);
  if ((self = [super init])) {
    handler_ = handler;
    certRequestInfo_ = certRequestInfo;
  }
  return self;
}

- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  net::X509Certificate* cert = NULL;

  if (returnCode == NSFileHandlingPanelOKButton) {
    NSUInteger index = [identities_ indexOfObject:(id)[panel_ identity]];
    DCHECK(index != NSNotFound);
    cert = certificates_[index];
  }

  // Finally, tell the backend which identity (or none) the user selected.
  handler_->CertificateSelected(cert);
  // Clean up. The retain occurred just before the sheet opened.
  [self release];
}

- (void)displayDialog:(gfx::NativeWindow)parent {
  DCHECK(!panel_.get());
  // Create an array of CFIdentityRefs for the certificates:
  size_t numCerts = certRequestInfo_->client_certs.size();
  identities_.reset([[NSMutableArray alloc] initWithCapacity:numCerts]);
  for (size_t i = 0; i < numCerts; ++i) {
    SecCertificateRef cert;
    cert = certRequestInfo_->client_certs[i]->os_cert_handle();
    SecIdentityRef identity;
    if (SecIdentityCreateWithCertificate(NULL, cert, &identity) == noErr) {
      [identities_ addObject:(id)identity];
      CFRelease(identity);
      certificates_.push_back(certRequestInfo_->client_certs[i]);
    }
  }

  // Get the message to display:
  NSString* title = l10n_util::GetNSString(IDS_CLIENT_CERT_DIALOG_TITLE);
  NSString* message = l10n_util::GetNSStringF(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      ASCIIToUTF16(certRequestInfo_->host_and_port));

  // Create and set up a system choose-identity panel.
  panel_.reset([[SFChooseIdentityPanel alloc] init]);
  NSString* domain = base::SysUTF8ToNSString(
      "https://" + certRequestInfo_->host_and_port);
  // Setting the domain causes the dialog to record the preferred
  // identity in the system keychain.
  [panel_ setDomain:domain];
  [panel_ setInformativeText:message];
  [panel_ setDefaultButtonTitle:l10n_util::GetNSString(IDS_OK)];
  [panel_ setAlternateButtonTitle:l10n_util::GetNSString(IDS_CANCEL)];
  SecPolicyRef sslPolicy;
  if (net::X509Certificate::CreateSSLClientPolicy(&sslPolicy) == noErr) {
    [panel_ setPolicies:(id)sslPolicy];
    CFRelease(sslPolicy);
  }

  // Increase the retain count to keep |self| alive while the sheet
  // runs. -sheetDidEnd:returnCode:context: will release the
  // additional reference.
  [self retain];
  if (parent) {
    // Open the cert panel as a sheet on the browser window.
    [panel_ beginSheetForWindow:parent
                  modalDelegate:self
                 didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                    contextInfo:nil
                    identities:identities_
                       message:title];
  } else {
    // No available browser window, so run independently as a (blocking) dialog.
    int returnCode = [panel_ runModalForIdentities:identities_ message:title];
    [self sheetDidEnd:panel_ returnCode:returnCode context:nil];
  }
}

@end
