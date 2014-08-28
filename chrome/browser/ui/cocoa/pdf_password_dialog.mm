// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "ui/base/l10n/l10n_util.h"

@class PDFPasswordDialogMac;

namespace {

class PDFPasswordDialogMacBridge : public ConstrainedWindowMacDelegate {
 public:
  explicit PDFPasswordDialogMacBridge(PDFPasswordDialogMac* dialog);
  virtual ~PDFPasswordDialogMacBridge();
  virtual void OnConstrainedWindowClosed(ConstrainedWindowMac* window) OVERRIDE;

 private:
  PDFPasswordDialogMac* dialog_;  // weak

  DISALLOW_COPY_AND_ASSIGN(PDFPasswordDialogMacBridge);
};

}  // namespace

@interface PDFPasswordDialogMac : NSObject {
 @private
  content::WebContents* webContents_;
  base::string16 prompt_;
  pdf::PasswordDialogClosedCallback callback_;

  base::scoped_nsobject<NSSecureTextField> passwordField_;

  base::scoped_nsobject<ConstrainedWindowAlert> alert_;
  scoped_ptr<PDFPasswordDialogMacBridge> bridge_;
  scoped_ptr<ConstrainedWindowMac> window_;
}
- (id)initWithWebContents:(content::WebContents*)webContents
                   prompt:(base::string16)prompt
                 callback:(pdf::PasswordDialogClosedCallback)callback;
- (void)onOKButton:(id)sender;
- (void)onCancelButton:(id)sender;
@end

namespace {

PDFPasswordDialogMacBridge::PDFPasswordDialogMacBridge(
    PDFPasswordDialogMac* dialog) : dialog_(dialog) {
}

PDFPasswordDialogMacBridge::~PDFPasswordDialogMacBridge() {
}

void PDFPasswordDialogMacBridge::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  [dialog_ release];
}

}  // namespace

@implementation PDFPasswordDialogMac

- (id)initWithWebContents:(content::WebContents*)webContents
                   prompt:(base::string16)prompt
                 callback:(pdf::PasswordDialogClosedCallback)callback {
  if ((self = [super init])) {
    webContents_ = webContents;
    prompt_ = prompt;
    callback_ = callback;

    alert_.reset([[ConstrainedWindowAlert alloc] init]);
    [alert_ setMessageText:
        l10n_util::GetNSString(IDS_PDF_PASSWORD_DIALOG_TITLE)];
    [alert_ setInformativeText:base::SysUTF16ToNSString(prompt)];
    [alert_ addButtonWithTitle:l10n_util::GetNSString(IDS_OK)
                 keyEquivalent:kKeyEquivalentReturn
                        target:self
                        action:@selector(onOKButton:)];
    [alert_ addButtonWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                 keyEquivalent:kKeyEquivalentEscape
                        target:self
                        action:@selector(onCancelButton:)];
    [[alert_ closeButton] setTarget:self];
    [[alert_ closeButton] setAction:@selector(onCancelButton:)];

    passwordField_.reset(
        [[NSSecureTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)]);
    [alert_ setAccessoryView:passwordField_];

    [alert_ layout];

    base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
        [[CustomConstrainedWindowSheet alloc]
            initWithCustomWindow:[alert_ window]]);
    bridge_.reset(new PDFPasswordDialogMacBridge(self));
    window_.reset(new ConstrainedWindowMac(bridge_.get(), webContents_, sheet));
  }
  return self;
}

- (void)dealloc {
  if (!callback_.is_null()) {
    // This dialog was torn down without either OK or cancel being clicked; be
    // considerate and at least do the callback.
    callback_.Run(false, base::string16());
  }
  [super dealloc];
}

- (void)onOKButton:(id)sender {
  callback_.Run(true, base::SysNSStringToUTF16([passwordField_ stringValue]));
  callback_.Reset();
  window_->CloseWebContentsModalDialog();
}

- (void)onCancelButton:(id)sender {
  callback_.Run(false, base::string16());
  callback_.Reset();
  window_->CloseWebContentsModalDialog();
}

@end

void ShowPDFPasswordDialog(content::WebContents* web_contents,
                           const base::string16& prompt,
                           const pdf::PasswordDialogClosedCallback& callback) {
  [[PDFPasswordDialogMac alloc] initWithWebContents:web_contents
                                             prompt:prompt
                                           callback:callback];
}
