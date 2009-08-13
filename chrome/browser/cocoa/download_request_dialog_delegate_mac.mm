// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/download_request_dialog_delegate_mac.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util_mac.h"
#include "app/message_box_flags.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

@interface SheetCallback : NSObject {
  DownloadRequestDialogDelegateMac* target_;  // weak
}
- (id)initWithTarget:(DownloadRequestDialogDelegateMac*)target;
- (void) alertDidEnd:(NSAlert *)alert
          returnCode:(int)returnCode
         contextInfo:(void *)contextInfo;
@end

@implementation SheetCallback
- (id)initWithTarget:(DownloadRequestDialogDelegateMac*)target {
  if ((self = [super init]) != nil) {
    target_ = target;
  }
  return self;
}
- (void)alertDidEnd:(NSAlert *)alert
         returnCode:(int)returnCode
        contextInfo:(void *)contextInfo {
  target_->SheetDidEnd(returnCode);
}
@end


// static
DownloadRequestDialogDelegate* DownloadRequestDialogDelegate::Create(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host) {
  return new DownloadRequestDialogDelegateMac(tab, host);
}

DownloadRequestDialogDelegateMac::DownloadRequestDialogDelegateMac(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host)
    : DownloadRequestDialogDelegate(host),
      ConstrainedWindowMacDelegateSystemSheet(
          [[[SheetCallback alloc] initWithTarget:this] autorelease],
          @selector(alertDidEnd:returnCode:contextInfo:)),
      responded_(false) {

  // Set up alert.
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_MULTI_DOWNLOAD_WARNING_ALLOW)];
  NSButton* denyButton = [alert addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_MULTI_DOWNLOAD_WARNING_DENY)];
  [alert setMessageText:
      l10n_util::GetNSStringWithFixup(IDS_MULTI_DOWNLOAD_WARNING)];
  [alert setAlertStyle:NSInformationalAlertStyle];

  // Cocoa automatically gives ESC as a key equivalent to buttons with the text
  // "Cancel". Since we use a different text, we have to set this ourselves.
  NSString* escapeKey = @"\e";
  [denyButton setKeyEquivalent:escapeKey];

  set_sheet(alert);

  // Display alert in a per-tab sheet.
  window_ = tab->CreateConstrainedDialog(this);
}

void DownloadRequestDialogDelegateMac::CloseWindow() {
  window_->CloseConstrainedWindow();
}

void DownloadRequestDialogDelegateMac::DeleteDelegate() {
  if (is_sheet_open()) {
    // Close sheet if it's still open.
    [NSApp endSheet:[(NSAlert*)sheet() window]];  // calls SheetDidEnd().
    DCHECK(responded_);
  }
  if (!responded_) {
    // Happens if the sheet was never visible.
    responded_ = true;
    DoCancel();
  }
  DCHECK(!host_);
  delete this;
}

void DownloadRequestDialogDelegateMac::SheetDidEnd(int returnCode) {
  DCHECK(!responded_);
  if (returnCode == NSAlertFirstButtonReturn) {
    DoAccept();
  } else {
    DoCancel();
  }
  responded_ = true;
}
