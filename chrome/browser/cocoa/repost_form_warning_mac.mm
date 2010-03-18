// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/repost_form_warning_mac.h"

#include "app/l10n_util_mac.h"
#include "base/message_loop.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

// The delegate of the NSAlert used to display the dialog. Forwards the alert's
// completion event to the C++ class |RepostFormWarningMac|.
@interface RepostDelegate : NSObject {
  RepostFormWarningMac* warning_;  // weak, owns us.
}
- (id)initWithWarning:(RepostFormWarningMac*)warning;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

@implementation RepostDelegate
- (id)initWithWarning:(RepostFormWarningMac*)warning {
  if ((self = [super init])) {
    warning_ = warning;
  }
  return self;
}

- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  if (returnCode == NSAlertFirstButtonReturn)
    warning_->Confirm();
  else
    warning_->Cancel();
}
@end

RepostFormWarningMac::RepostFormWarningMac(
    NSWindow* parent,
    TabContents* tab_contents)
    : ConstrainedWindowMacDelegateSystemSheet(
        [[[RepostDelegate alloc] initWithWarning:this] autorelease],
        @selector(alertDidEnd:returnCode:contextInfo:)),
      navigation_controller_(&tab_contents->controller()) {
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

  registrar_.Add(this, NotificationType::LOAD_START,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::RELOADING,
                 Source<NavigationController>(navigation_controller_));
  window_ = tab_contents->CreateConstrainedDialog(this);
}

RepostFormWarningMac::~RepostFormWarningMac() {
}

void RepostFormWarningMac::DeleteDelegate() {
  window_ = NULL;
  Dismiss();
  delete this;
}

void RepostFormWarningMac::Confirm() {
  if (navigation_controller_) {
    navigation_controller_->ContinuePendingReload();
  }
  Destroy();
}

void RepostFormWarningMac::Cancel() {
  if (navigation_controller_) {
    navigation_controller_->CancelPendingReload();
  }
  Destroy();
}

void RepostFormWarningMac::Dismiss() {
  if (sheet() && is_sheet_open()) {
    // This will call |Cancel()|.
    [NSApp endSheet:[(NSAlert*)sheet() window]
         returnCode:NSAlertSecondButtonReturn];
  }
}

void RepostFormWarningMac::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if ((type == NotificationType::LOAD_START ||
       type == NotificationType::TAB_CLOSING ||
       type == NotificationType::RELOADING)) {
    DCHECK_EQ(Source<NavigationController>(source).ptr(),
              navigation_controller_);
    Dismiss();
  }
}

void RepostFormWarningMac::Destroy() {
  navigation_controller_ = NULL;
  if (sheet()) {
    set_sheet(nil);
  }
  if (window_) {
    window_->CloseConstrainedWindow();
  }
}
