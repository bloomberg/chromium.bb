// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/certificate_viewer_mac.h"

#include <Security/Security.h>
#include <SecurityInterface/SFCertificatePanel.h>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "chrome/browser/certificate_viewer.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_mac.h"
#import "ui/base/cocoa/window_size_constants.h"

class SSLCertificateViewerCocoaBridge;

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

@interface SSLCertificateViewerCocoa ()
- (void)onConstrainedWindowClosed;
@end

class SSLCertificateViewerCocoaBridge : public ConstrainedWindowMacDelegate {
 public:
  explicit SSLCertificateViewerCocoaBridge(SSLCertificateViewerCocoa *
                                           controller)
      : controller_(controller) {
  }

  virtual ~SSLCertificateViewerCocoaBridge() {}

  // ConstrainedWindowMacDelegate implementation:
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override {
    // |onConstrainedWindowClosed| will delete the sheet which might be still
    // in use higher up the call stack. Wait for the next cycle of the event
    // loop to call this function.
    [controller_ performSelector:@selector(onConstrainedWindowClosed)
                      withObject:nil
                      afterDelay:0];
  }

 private:
  SSLCertificateViewerCocoa* controller_;  // weak

  DISALLOW_COPY_AND_ASSIGN(SSLCertificateViewerCocoaBridge);
};

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  // SSLCertificateViewerCocoa will manage its own lifetime and will release
  // itself when the dialog is closed.
  // See -[SSLCertificateViewerCocoa onConstrainedWindowClosed].
  SSLCertificateViewerCocoa* viewer =
      [[SSLCertificateViewerCocoa alloc] initWithCertificate:cert];
  [viewer displayForWebContents:web_contents];
}

@implementation SSLCertificateViewerCocoa

- (id)initWithCertificate:(net::X509Certificate*)certificate {
  if ((self = [super init])) {
    base::ScopedCFTypeRef<CFArrayRef> cert_chain(
        certificate->CreateOSCertChainForCert());
    NSArray* certificates = base::mac::CFToNSCast(cert_chain.get());
    certificates_.reset([certificates retain]);
  }
  return self;
}

- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  if (!closePending_)
    constrainedWindow_->CloseWebContentsModalDialog();
}

- (void)displayForWebContents:(content::WebContents*)webContents {
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
    return;
  }
  // Add a basic X.509 policy, in order to match the behaviour of
  // SFCertificatePanel when no policies are specified.
  SecPolicyRef basic_policy = NULL;
  OSStatus status = net::x509_util::CreateBasicX509Policy(&basic_policy);
  if (status != noErr) {
    NOTREACHED();
    return;
  }
  CFArrayAppendValue(policies, basic_policy);
  CFRelease(basic_policy);

  status = net::x509_util::CreateRevocationPolicies(false, false, policies);
  if (status != noErr) {
    NOTREACHED();
    return;
  }

  panel_.reset([[SFCertificatePanel alloc] init]);
  [panel_ setPolicies:(id) policies.get()];

  constrainedWindow_.reset(
      new ConstrainedWindowMac(observer_.get(), webContents, self));
}

- (NSWindow*)overlayWindow {
  return overlayWindow_;
}

- (void)showSheetForWindow:(NSWindow*)window {
  overlayWindow_.reset([window retain]);
  [panel_ beginSheetForWindow:window
                modalDelegate:self
               didEndSelector:@selector(sheetDidEnd:
                                         returnCode:
                                            context:)
                  contextInfo:NULL
                 certificates:certificates_
                    showGroup:YES];
}

- (void)closeSheetWithAnimation:(BOOL)withAnimation {
  closePending_ = YES;
  overlayWindow_.reset();
  // Closing the sheet using -[NSApp endSheet:] doesn't work so use the private
  // method.
  [panel_ _dismissWithCode:NSFileHandlingPanelCancelButton];
}

- (void)hideSheet {
  NSWindow* sheetWindow = [overlayWindow_ attachedSheet];
  [sheetWindow setAlphaValue:0.0];

  oldResizesSubviews_ = [[sheetWindow contentView] autoresizesSubviews];
  [[sheetWindow contentView] setAutoresizesSubviews:NO];
}

- (void)unhideSheet {
  NSWindow* sheetWindow = [overlayWindow_ attachedSheet];

  [[sheetWindow contentView] setAutoresizesSubviews:oldResizesSubviews_];
  [[overlayWindow_ attachedSheet] setAlphaValue:1.0];
}

- (void)pulseSheet {
  // NOOP
}

- (void)makeSheetKeyAndOrderFront {
  [[overlayWindow_ attachedSheet] makeKeyAndOrderFront:nil];
}

- (void)updateSheetPosition {
  // NOOP
}

- (NSWindow*)sheetWindow {
  return panel_;
}

- (void)onConstrainedWindowClosed {
  panel_.reset();
  constrainedWindow_.reset();
  [self release];
}

@end
