// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/ssl_client_certificate_selector_cocoa.h"

#import <SecurityInterface/SFChooseIdentityPanel.h>
#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ssl/ssl_client_auth_observer.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_mac.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"

using content::BrowserThread;

@interface SFChooseIdentityPanel (SystemPrivate)
// A system-private interface that dismisses a panel whose sheet was started by
// -beginSheetForWindow:modalDelegate:didEndSelector:contextInfo:identities:message:
// as though the user clicked the button identified by returnCode. Verified
// present in 10.5 through 10.8.
- (void)_dismissWithCode:(NSInteger)code;
@end

@interface SSLClientCertificateSelectorCocoa ()
- (void)onConstrainedWindowClosed;
@end

class SSLClientAuthObserverCocoaBridge : public SSLClientAuthObserver,
                                         public ConstrainedWindowMacDelegate {
 public:
  SSLClientAuthObserverCocoaBridge(
      const content::BrowserContext* browser_context,
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      SSLClientCertificateSelectorCocoa* controller)
      : SSLClientAuthObserver(browser_context,
                              cert_request_info,
                              std::move(delegate)),
        controller_(controller) {}

  // SSLClientAuthObserver implementation:
  void OnCertSelectedByNotification() override {
    [controller_ closeWebContentsModalDialog];
  }

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
  SSLClientCertificateSelectorCocoa* controller_;  // weak
};

namespace chrome {

void ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Not all WebContentses can show modal dialogs.
  //
  // Use the top-level embedder if |contents| is a guest.
  // GetTopLevelWebContents() will return |contents| otherwise.
  // TODO(davidben): Move this hook to the WebContentsDelegate and only try to
  // show a dialog in Browser's implementation. https://crbug.com/456255
  if (web_modal::WebContentsModalDialogManager::FromWebContents(
          guest_view::GuestViewBase::GetTopLevelWebContents(contents)) ==
      nullptr)
    return;

  // The dialog manages its own lifetime.
  SSLClientCertificateSelectorCocoa* selector =
      [[SSLClientCertificateSelectorCocoa alloc]
          initWithBrowserContext:contents->GetBrowserContext()
                 certRequestInfo:cert_request_info
                        delegate:std::move(delegate)];
  [selector displayForWebContents:contents];
}

}  // namespace chrome

namespace {

// These ClearTableViewDataSources... functions help work around a bug in macOS
// 10.12 where SFChooseIdentityPanel leaks a window and some views, including
// an NSTableView. Future events may make cause the table view to query its
// dataSource, which will have been deallocated.
//
// NSTableView.dataSource becomes a zeroing weak reference starting in 10.11,
// so this workaround can be removed once we're on the 10.11 SDK.
//
// See https://crbug.com/653093 and rdar://29409207 for more information.

#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11

void ClearTableViewDataSources(NSView* view) {
  if (auto table_view = base::mac::ObjCCast<NSTableView>(view)) {
    table_view.dataSource = nil;
  } else {
    for (NSView* subview in view.subviews) {
      ClearTableViewDataSources(subview);
    }
  }
}

void ClearTableViewDataSourcesIfWindowStillExists(NSWindow* leaked_window) {
  for (NSWindow* window in [NSApp windows]) {
    // If the window is still in the window list...
    if (window == leaked_window) {
      // ...search it for table views.
      ClearTableViewDataSources(window.contentView);
      break;
    }
  }
}

void ClearTableViewDataSourcesIfNeeded(NSWindow* leaked_window) {
  // Let the autorelease pool drain before deciding if the window was leaked.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(ClearTableViewDataSourcesIfWindowStillExists,
                            base::Unretained(leaked_window)));
}

#else

void ClearTableViewDataSourcesIfNeeded(NSWindow*) {}

#endif  // MAC_OS_X_VERSION_10_11

}  // namespace

@implementation SSLClientCertificateSelectorCocoa

- (id)initWithBrowserContext:(const content::BrowserContext*)browserContext
             certRequestInfo:(net::SSLCertRequestInfo*)certRequestInfo
                    delegate:
                        (std::unique_ptr<content::ClientCertificateDelegate>)
                            delegate {
  DCHECK(browserContext);
  DCHECK(certRequestInfo);
  if ((self = [super init])) {
    observer_.reset(new SSLClientAuthObserverCocoaBridge(
        browserContext, certRequestInfo, std::move(delegate), self));
  }
  return self;
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  net::X509Certificate* cert = NULL;
  if (returnCode == NSFileHandlingPanelOKButton) {
    CFRange range = CFRangeMake(0, CFArrayGetCount(identities_));
    CFIndex index =
        CFArrayGetFirstIndexOfValue(identities_, range, [panel_ identity]);
    if (index != -1)
      cert = certificates_[index].get();
    else
      NOTREACHED();
  }

  // See comment at definition; this works around a 10.12 bug.
  ClearTableViewDataSourcesIfNeeded(sheet);

  if (!closePending_) {
    // If |closePending_| is already set, |closeSheetWithAnimation:| was called
    // already to cancel the selection rather than continue with no
    // certificate. Otherwise, tell the backend which identity (or none) the
    // user selected.
    userResponded_ = YES;
    observer_->CertificateSelected(cert);

    constrainedWindow_->CloseWebContentsModalDialog();
  }
}

- (void)displayForWebContents:(content::WebContents*)webContents {
  // Create an array of CFIdentityRefs for the certificates:
  size_t numCerts = observer_->cert_request_info()->client_certs.size();
  identities_.reset(CFArrayCreateMutable(
      kCFAllocatorDefault, numCerts, &kCFTypeArrayCallBacks));
  for (size_t i = 0; i < numCerts; ++i) {
    SecCertificateRef cert =
        observer_->cert_request_info()->client_certs[i]->os_cert_handle();
    SecIdentityRef identity;
    if (SecIdentityCreateWithCertificate(NULL, cert, &identity) == noErr) {
      CFArrayAppendValue(identities_, identity);
      CFRelease(identity);
      certificates_.push_back(observer_->cert_request_info()->client_certs[i]);
    }
  }

  // Get the message to display:
  NSString* message = l10n_util::GetNSStringF(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      base::ASCIIToUTF16(
          observer_->cert_request_info()->host_and_port.ToString()));

  // Create and set up a system choose-identity panel.
  panel_.reset([[SFChooseIdentityPanel alloc] init]);
  [panel_ setInformativeText:message];
  [panel_ setDefaultButtonTitle:l10n_util::GetNSString(IDS_OK)];
  [panel_ setAlternateButtonTitle:l10n_util::GetNSString(IDS_CANCEL)];
  SecPolicyRef sslPolicy;
  if (net::x509_util::CreateSSLClientPolicy(&sslPolicy) == noErr) {
    [panel_ setPolicies:(id)sslPolicy];
    CFRelease(sslPolicy);
  }

  constrainedWindow_ =
      CreateAndShowWebModalDialogMac(observer_.get(), webContents, self);
  observer_->StartObserving();
}

- (void)closeWebContentsModalDialog {
  DCHECK(constrainedWindow_);
  constrainedWindow_->CloseWebContentsModalDialog();
}

- (NSWindow*)overlayWindow {
  return overlayWindow_;
}

- (SFChooseIdentityPanel*)panel {
  return panel_;
}

- (void)showSheetForWindow:(NSWindow*)window {
  NSString* title = l10n_util::GetNSString(IDS_CLIENT_CERT_DIALOG_TITLE);
  overlayWindow_.reset([window retain]);
  [panel_ beginSheetForWindow:window
                modalDelegate:self
               didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                  contextInfo:NULL
                   identities:base::mac::CFToNSCast(identities_)
                      message:title];
}

- (void)closeSheetWithAnimation:(BOOL)withAnimation {
  if (!userResponded_) {
    // If the sheet is closed by closing the tab rather than the user explicitly
    // hitting Cancel, |closeSheetWithAnimation:| gets called before
    // |sheetDidEnd:|. In this case, the selection should be canceled rather
    // than continue with no certificate. The |returnCode| parameter to
    // |sheetDidEnd:| is the same in both cases.
    observer_->CancelCertificateSelection();
  }
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

  oldSheetFrame_ = [sheetWindow frame];
  NSRect overlayFrame = [overlayWindow_ frame];
  oldSheetFrame_.origin.x -= NSMinX(overlayFrame);
  oldSheetFrame_.origin.y -= NSMinY(overlayFrame);
  [sheetWindow setFrame:ui::kWindowSizeDeterminedLater display:NO];
}

- (void)unhideSheet {
  NSWindow* sheetWindow = [overlayWindow_ attachedSheet];
  NSRect overlayFrame = [overlayWindow_ frame];
  oldSheetFrame_.origin.x += NSMinX(overlayFrame);
  oldSheetFrame_.origin.y += NSMinY(overlayFrame);
  [sheetWindow setFrame:oldSheetFrame_ display:NO];
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

- (void)resizeWithNewSize:(NSSize)size {
  // NOOP
}

- (NSWindow*)sheetWindow {
  return panel_;
}

- (void)onConstrainedWindowClosed {
  observer_->StopObserving();
  panel_.reset();
  constrainedWindow_.reset();
  [self release];
}

@end
