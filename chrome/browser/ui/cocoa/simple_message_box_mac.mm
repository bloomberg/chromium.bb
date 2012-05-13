// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#import <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace browser {

void ShowWarningMessageBox(gfx::NativeWindow parent,
                           const string16& title,
                           const string16& message) {
  // Ignore the title; it's the window title on other platforms and ignorable.
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert addButtonWithTitle:l10n_util::GetNSString(IDS_OK)];
  [alert setMessageText:base::SysUTF16ToNSString(message)];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert runModal];
}

bool ShowQuestionMessageBox(gfx::NativeWindow parent,
                            const string16& title,
                            const string16& message) {
  // Ignore the title; it's the window title on other platforms and ignorable.
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:base::SysUTF16ToNSString(message)];
  [alert setAlertStyle:NSWarningAlertStyle];

  [alert addButtonWithTitle:
      l10n_util::GetNSString(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)];
  [alert addButtonWithTitle:
      l10n_util::GetNSString(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL)];

  NSInteger result = [alert runModal];
  return result == NSAlertFirstButtonReturn;
}

}  // namespace browser
