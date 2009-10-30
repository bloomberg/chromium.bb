// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/repost_form_warning_mac.h"

#include "app/l10n_util_mac.h"
#include "base/message_loop.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

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
    NavigationController* navigation_controller)
    : navigation_controller_(navigation_controller),
      alert_([[NSAlert alloc] init]),
      delegate_([[RepostDelegate alloc] initWithWarning:this]) {
  [alert_ setMessageText:
      l10n_util::GetNSStringWithFixup(IDS_HTTP_POST_WARNING_TITLE)];
  [alert_ setInformativeText:
      l10n_util::GetNSStringWithFixup(IDS_HTTP_POST_WARNING)];
  [alert_ addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_HTTP_POST_WARNING_RESEND)];
  [alert_ addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_HTTP_POST_WARNING_CANCEL)];

  [alert_ beginSheetModalForWindow:parent
                    modalDelegate:delegate_.get()
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                      contextInfo:nil];

  registrar_.Add(this, NotificationType::LOAD_START,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(navigation_controller_));
}

RepostFormWarningMac::~RepostFormWarningMac() {
}

void RepostFormWarningMac::Confirm() {
  Destroy();
  if (navigation_controller_)
    navigation_controller_->Reload(false);
}

void RepostFormWarningMac::Cancel() {
  Destroy();
}

void RepostFormWarningMac::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if (alert_ && navigation_controller_ &&
      (type == NotificationType::LOAD_START ||
       type == NotificationType::TAB_CLOSING)) {
    DCHECK_EQ(Source<NavigationController>(source).ptr(),
              navigation_controller_);
    navigation_controller_ = NULL;

    // This will call |Cancel()|.
    [NSApp endSheet:[alert_ window] returnCode:NSAlertSecondButtonReturn];
  }
}

void RepostFormWarningMac::Destroy() {
  if (alert_) {
    alert_.reset(NULL);
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}
