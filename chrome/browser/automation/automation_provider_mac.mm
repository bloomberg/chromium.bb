// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/cocoa/tab_window_controller.h"
#include "chrome/browser/view_ids.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/test/automation/automation_messages.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "grit/generated_resources.h"

void AutomationProvider::PrintAsync(int tab_handle) {
  NOTIMPLEMENTED();
}

void AutomationProvider::WindowSimulateDrag(int handle,
                                            std::vector<gfx::Point> drag_path,
                                            int flags,
                                            bool press_escape_en_route,
                                            IPC::Message* reply_message) {
  NOTIMPLEMENTED();
  AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void AutomationProvider::GetWindowTitle(int handle, string16* text) {
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  NSString* title = nil;
  if ([[window delegate] isKindOfClass:[TabWindowController class]]) {
    TabWindowController* delegate =
        reinterpret_cast<TabWindowController*>([window delegate]);
    title = [delegate selectedTabTitle];
  } else {
    title = [window title];
  }
  // If we don't yet have a title, use "Untitled".
  if (![title length]) {
    text->assign(l10n_util::GetStringUTF16(
        IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));
    return;
  }

  text->assign(base::SysNSStringToUTF16(title));
}
