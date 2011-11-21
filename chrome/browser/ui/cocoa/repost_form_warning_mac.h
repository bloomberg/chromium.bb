// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_REPOST_FORM_WARNING_MAC_H_
#define CHROME_BROWSER_UI_COCOA_REPOST_FORM_WARNING_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/cocoa/constrained_window_mac.h"

class RepostFormWarningController;
class TabContents;

// Displays a dialog that warns the user that they are about to resubmit
// a form. To show the dialog, call the |Create| method. It will open the
// dialog and then delete itself when the user dismisses the dialog.
class RepostFormWarningMac : public ConstrainedWindowMacDelegateSystemSheet {
 public:
  // Convenience method that creates a new |RepostFormWarningController| and
  // then a new |RepostFormWarningMac| from that.
  static RepostFormWarningMac* Create(NSWindow* parent,
                                      TabContents* tab_contents);

  RepostFormWarningMac(NSWindow* parent,
                       RepostFormWarningController* controller,
                       TabContents* tab_contents);

  // ConstrainedWindowDelegateMacSystemSheet methods:
  virtual void DeleteDelegate() OVERRIDE;

 private:
  virtual ~RepostFormWarningMac();

  scoped_ptr<RepostFormWarningController> controller_;
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_REPOST_FORM_WARNING_MAC_H_
