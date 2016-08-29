// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/certificate_viewer_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_mac.h"

@interface SFCertificatePanel (SystemPrivate)
// A system-private interface that dismisses a panel whose sheet was started by
// -beginSheetForWindow:
//        modalDelegate:
//       didEndSelector:
//          contextInfo:
//         certificates:
//            showGroup:
// as though the user clicked the button identified by returnCode. Verified
// present in 10.8.
- (void)_dismissWithCode:(NSInteger)code;
@end

@implementation SSLCertificateViewerMac {
  // The corresponding list of certificates.
  base::scoped_nsobject<NSArray> certificates_;
  base::scoped_nsobject<SFCertificatePanel> panel_;
}

- (instancetype)initWithCertificate:(net::X509Certificate*)certificate
                     forWebContents:(content::WebContents*)webContents {
  if ((self = [super init])) {
    base::ScopedCFTypeRef<CFArrayRef> certChain(
        certificate->CreateOSCertChainForCert());
    NSArray* certificates = base::mac::CFToNSCast(certChain.get());
    certificates_.reset([certificates retain]);
  }

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
  base::ScopedCFTypeRef<CFMutableArrayRef> policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!policies.get()) {
    NOTREACHED();
    return self;
  }
  // Add a basic X.509 policy, in order to match the behaviour of
  // SFCertificatePanel when no policies are specified.
  SecPolicyRef basicPolicy = nil;
  OSStatus status = net::x509_util::CreateBasicX509Policy(&basicPolicy);
  if (status != noErr) {
    NOTREACHED();
    return self;
  }
  CFArrayAppendValue(policies, basicPolicy);
  CFRelease(basicPolicy);

  status = net::x509_util::CreateRevocationPolicies(false, false, policies);
  if (status != noErr) {
    NOTREACHED();
    return self;
  }

  panel_.reset([[SFCertificatePanel alloc] init]);
  [panel_ setPolicies:base::mac::CFToNSCast(policies.get())];
  return self;
}

- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  NOTREACHED();  // Subclasses must implement this.
}

- (void)showCertificateSheet:(NSWindow*)window {
  [panel_ beginSheetForWindow:window
                modalDelegate:self
               didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                  contextInfo:nil
                 certificates:certificates_
                    showGroup:YES];
}

- (void)closeCertificateSheet {
  // Closing the sheet using -[NSApp endSheet:] doesn't work so use the private
  // method.
  [panel_ _dismissWithCode:NSFileHandlingPanelCancelButton];
  certificates_.reset();
}

- (void)releaseSheetWindow {
  panel_.reset();
}

- (NSWindow*)certificatePanel {
  return panel_;
}

@end
