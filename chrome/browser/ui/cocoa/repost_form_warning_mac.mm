// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/repost_form_warning_mac.h"

#include "base/scoped_nsobject.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

// The delegate of the NSAlert used to display the dialog. Forwards the alert's
// completion event to the C++ class |RepostFormWarningController|.
@interface RepostDelegate : NSObject {
  RepostFormWarningController* warning_;  // weak
}
- (id)initWithWarning:(RepostFormWarningController*)warning;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

@implementation RepostDelegate
- (id)initWithWarning:(RepostFormWarningController*)warning {
  if ((self = [super init])) {
    warning_ = warning;
  }
  return self;
}

- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  if (returnCode == NSAlertFirstButtonReturn) {
    warning_->Continue();
  } else {
    warning_->Cancel();
  }
}
@end

RepostFormWarningMac* RepostFormWarningMac::Create(NSWindow* parent,
                                                  TabContents* tab_contents) {
  return new RepostFormWarningMac(
      parent,
      new RepostFormWarningController(tab_contents));
}

RepostFormWarningMac::RepostFormWarningMac(
    NSWindow* parent,
    RepostFormWarningController* controller)
    : ConstrainedWindowMacDelegateSystemSheet(
        [[[RepostDelegate alloc] initWithWarning:controller]
            autorelease],
        @selector(alertDidEnd:returnCode:contextInfo:)),
      controller_(controller) {
  scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:
      l10n_util::GetNSStringWithFixup(IDS_HTTP_POST_WARNING_TITLE)];
  [alert setInformativeText:
      l10n_util::GetNSStringWithFixup(IDS_HTTP_POST_WARNING)];
  [alert addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_HTTP_POST_WARNING_RESEND)];
  [alert addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_CANCEL)];

  set_sheet(alert);

  controller->Show(this);
}

RepostFormWarningMac::~RepostFormWarningMac() {
  NSWindow* window = [(NSAlert*)sheet() window];
  if (window && is_sheet_open()) {
    [NSApp endSheet:window
         returnCode:NSAlertSecondButtonReturn];
  }
}

void RepostFormWarningMac::DeleteDelegate() {
  delete this;
}
