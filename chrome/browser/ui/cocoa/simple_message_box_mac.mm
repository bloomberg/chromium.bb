// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace chrome {

MessageBoxResult ShowMessageBox(gfx::NativeWindow parent,
                                const base::string16& title,
                                const base::string16& message,
                                MessageBoxType type) {
  if (type == MESSAGE_BOX_TYPE_OK_CANCEL)
    NOTIMPLEMENTED();

  startup_metric_utils::SetNonBrowserUIDisplayed();

  // Ignore the title; it's the window title on other platforms and ignorable.
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:base::SysUTF16ToNSString(message)];
  NSUInteger style = (type == MESSAGE_BOX_TYPE_INFORMATION) ?
      NSInformationalAlertStyle : NSWarningAlertStyle;
  [alert setAlertStyle:style];
  if (type == MESSAGE_BOX_TYPE_QUESTION) {
    [alert addButtonWithTitle:
        l10n_util::GetNSString(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)];
    [alert addButtonWithTitle:
        l10n_util::GetNSString(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL)];
  } else {
    [alert addButtonWithTitle:l10n_util::GetNSString(IDS_OK)];
  }
  NSInteger result = [alert runModal];
  return (result == NSAlertSecondButtonReturn) ?
      MESSAGE_BOX_RESULT_NO : MESSAGE_BOX_RESULT_YES;
}

}  // namespace chrome
