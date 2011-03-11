// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#import <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/automation_messages.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

void AutomationProvider::PrintAsync(int tab_handle) {
  NOTIMPLEMENTED();
}

void AutomationProvider::WindowSimulateDrag(
    int handle,
    const std::vector<gfx::Point>& drag_path,
    int flags,
    bool press_escape_en_route,
    IPC::Message* reply_message) {
  NOTIMPLEMENTED();
  AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

