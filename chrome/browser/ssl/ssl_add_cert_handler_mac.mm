// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ssl_add_cert_handler.h"

#include <SecurityInterface/SFCertificatePanel.h>
#include <SecurityInterface/SFCertificateView.h>

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface SSLAddCertHandlerCocoa : NSObject
{
  scoped_refptr<SSLAddCertHandler> handler_;
}

- (id)initWithHandler:(SSLAddCertHandler*)handler;
- (void)askToAddCert;
@end


void SSLAddCertHandler::AskToAddCert() {
  [[[SSLAddCertHandlerCocoa alloc] initWithHandler: this] askToAddCert];
  // The new object will release itself when the sheet ends.
}


// The actual implementation of the add-client-cert handler is an Obj-C class.
@implementation SSLAddCertHandlerCocoa

- (id)initWithHandler:(SSLAddCertHandler*)handler {
  DCHECK(handler && handler->cert());
  self = [super init];
  if (self) {
    handler_ = handler;
  }
  return self;
}

- (void)sheetDidEnd:(SFCertificatePanel*)panel
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  [panel orderOut:self];
  [panel autorelease];
  handler_->Finished(returnCode == NSOKButton);
  [self release];
}

- (void)askToAddCert {
  NSWindow* parentWindow = NULL;
  Browser* browser = BrowserList::GetLastActive();
  // TODO(snej): Can I get the Browser that issued the request?
  if (browser) {
    parentWindow = browser->window()->GetNativeHandle();
    if ([parentWindow attachedSheet])
      parentWindow = nil;
  }

  // Create the cert panel, which will be released in my -sheetDidEnd: method.
  SFCertificatePanel* panel = [[SFCertificatePanel alloc] init];
  [panel setDefaultButtonTitle:l10n_util::GetNSString(IDS_ADD_CERT_DIALOG_ADD)];
  [panel setAlternateButtonTitle:l10n_util::GetNSString(IDS_CANCEL)];
  SecCertificateRef cert = handler_->cert()->os_cert_handle();
  NSArray* certs = [NSArray arrayWithObject: (id)cert];

  if (parentWindow) {
    // Open the cert panel as a sheet on the browser window.
    [panel beginSheetForWindow:parentWindow
                 modalDelegate:self
                didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                   contextInfo:NULL
                  certificates:certs
                     showGroup:NO];
  } else {
    // No available browser window, so run independently as a (blocking) dialog.
    int returnCode = [panel runModalForCertificates:certs showGroup:NO];
    [self sheetDidEnd:panel returnCode:returnCode context:NULL];
  }
}

@end
