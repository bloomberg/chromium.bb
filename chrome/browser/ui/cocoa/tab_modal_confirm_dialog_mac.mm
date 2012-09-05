// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_modal_confirm_dialog_mac.h"

#include "base/command_line.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac2.h"
#include "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"

// The delegate of the NSAlert used to display the dialog. Forwards the alert's
// completion event to the C++ class |TabModalConfirmDialogDelegate|.
@interface TabModalConfirmDialogMacBridge : NSObject {
  TabModalConfirmDialogDelegate* delegate_;  // weak
}
- (id)initWithDelegate:(TabModalConfirmDialogDelegate*)delegate;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

@implementation TabModalConfirmDialogMacBridge
- (id)initWithDelegate:(TabModalConfirmDialogDelegate*)delegate {
  if ((self = [super init])) {
    delegate_ = delegate;
  }
  return self;
}

- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  if (returnCode == NSAlertFirstButtonReturn) {
    delegate_->Accept();
  } else {
    delegate_->Cancel();
  }
}
@end

namespace chrome {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowTabModalConfirmDialog(TabModalConfirmDialogDelegate* delegate,
                               TabContents* tab_contents) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableFramelessConstrainedDialogs)) {
    // Deletes itself when closed.
    new TabModalConfirmDialogMac2(delegate, tab_contents);
  } else {
    // Deletes itself when closed.
    new TabModalConfirmDialogMac(delegate, tab_contents);
  }
}

}  // namespace chrome

TabModalConfirmDialogMac::TabModalConfirmDialogMac(
    TabModalConfirmDialogDelegate* delegate,
    TabContents* tab_contents)
    : ConstrainedWindowMacDelegateSystemSheet(
        [[[TabModalConfirmDialogMacBridge alloc] initWithDelegate:delegate]
            autorelease],
        @selector(alertDidEnd:returnCode:contextInfo:)),
      delegate_(delegate) {
  scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetTitle())];
  [alert setInformativeText:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetMessage())];
  [alert addButtonWithTitle:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetAcceptButtonTitle())];
  [alert addButtonWithTitle:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetCancelButtonTitle())];
  gfx::Image* icon = delegate->GetIcon();
  if (icon)
    [alert setIcon:icon->ToNSImage()];

  set_sheet(alert);

  delegate->set_window(new ConstrainedWindowMac(tab_contents, this));
}

TabModalConfirmDialogMac::~TabModalConfirmDialogMac() {
  NSWindow* window = [(NSAlert*)sheet() window];
  if (window && is_sheet_open()) {
    [NSApp endSheet:window
         returnCode:NSAlertSecondButtonReturn];
  }
}

// "DeleteDelegate" refers to this class being a ConstrainedWindow delegate
// and deleting itself, not to deleting the member variable |delegate_|.
void TabModalConfirmDialogMac::DeleteDelegate() {
  delete this;
}

@interface TabModalConfirmDialogMacBridge2 : NSObject {
  TabModalConfirmDialogDelegate* delegate_;  // weak
}
@end

@implementation TabModalConfirmDialogMacBridge2

- (id)initWithDelegate:(TabModalConfirmDialogDelegate*)delegate {
  if ((self = [super init])) {
    delegate_ = delegate;
    DCHECK(delegate_);
  }
  return self;
}

- (void)onAcceptButton:(id)sender {
  delegate_->Accept();
}

- (void)onCancelButton:(id)sender {
  delegate_->Cancel();
}

@end

TabModalConfirmDialogMac2::TabModalConfirmDialogMac2(
    TabModalConfirmDialogDelegate* delegate,
    TabContents* tab_contents)
    : delegate_(delegate) {
  bridge_.reset([[TabModalConfirmDialogMacBridge2 alloc]
      initWithDelegate:delegate]);

  alert_.reset([[ConstrainedWindowAlert alloc] init]);
  [alert_ setMessageText:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetTitle())];
  [alert_ setInformativeText:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetMessage())];
  [alert_ addButtonWithTitle:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetAcceptButtonTitle())
              keyEquivalent:kKeyEquivalentReturn
                     target:bridge_
                     action:@selector(onAcceptButton:)];
  [alert_ addButtonWithTitle:
      l10n_util::FixUpWindowsStyleLabel(delegate->GetCancelButtonTitle())
              keyEquivalent:kKeyEquivalentEscape
                     target:bridge_
                     action:@selector(onCancelButton:)];
  [[alert_ closeButton] setTarget:bridge_];
  [[alert_ closeButton] setAction:@selector(onCancelButton:)];
  [alert_ layout];

  delegate->set_window(
      new ConstrainedWindowMac2(tab_contents, [alert_ window]));
}

TabModalConfirmDialogMac2::~TabModalConfirmDialogMac2() {
}
