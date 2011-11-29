// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_TRY_CHROME_DIALOG_VIEW_H_
#define CHROME_BROWSER_FIRST_RUN_TRY_CHROME_DIALOG_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class ProcessSingleton;

namespace gfx {
class Rect;
}

namespace views {
class RadioButton;
class Widget;
}

// This class displays a modal dialog using the views system. The dialog asks
// the user to give chrome another try. This class only handles the UI so the
// resulting actions are up to the caller. One version looks like this:
//
//   +-----------------------------------------------+
//   | |icon| You stopped using Google Chrome    [x] |
//   | |icon| Would you like to:                     |
//   |        [o] Give the new version a try         |
//   |        [ ] Uninstall Google Chrome            |
//   |        [ OK ] [Don't bug me]                  |
//   |                                               |
//   |        _why_am_I_seeing this?_                |
//   +-----------------------------------------------+
//
class TryChromeDialogView : public views::ButtonListener,
                            public views::LinkListener {
 public:
  enum Result {
    TRY_CHROME,          // Launch chrome right now.
    NOT_NOW,             // Don't launch chrome. Exit now.
    UNINSTALL_CHROME,    // Initiate chrome uninstall and exit.
    DIALOG_ERROR,        // An error occurred creating the dialog.
    COUNT
  };

  // Shows a modal dialog asking the user to give chrome another try. See
  // above for the possible outcomes of the function. This is an experimental,
  // non-localized dialog.
  // |version| can be 0, 1 or 2 and selects what strings to present.
  // |process_singleton| needs to be valid and it will be locked while
  // the dialog is shown.
  // Note that the dialog has no parent and it will position itself in a lower
  // corner of the screen. The dialog does not steal focus and does not have an
  // entry in the taskbar.
  static Result Show(size_t version, ProcessSingleton* process_singleton);

 private:
  enum ButtonTags {
    BT_NONE,
    BT_CLOSE_BUTTON,
    BT_OK_BUTTON,
  };

  explicit TryChromeDialogView(size_t version);
  virtual ~TryChromeDialogView();

  Result ShowModal(ProcessSingleton* process_singleton);

  // Returns a screen rectangle that is fit to show the window. In particular
  // it has the following properties: a) is visible and b) is attached to the
  // bottom of the working area. For LTR machines it returns a left side
  // rectangle and for RTL it returns a right side rectangle so that the dialog
  // does not compete with the standar place of the start menu.
  gfx::Rect ComputeWindowPosition(int width, int height, bool is_RTL);

  // Create a windows region that looks like a toast of width |w| and height
  // |h|. This is best effort, so we don't care much if the operation fails.
  void SetToastRegion(gfx::NativeWindow window, int w, int h);

  // views::ButtonListener:
  // We have two buttons and according to what the user clicked we set |result_|
  // and we should always close and end the modal loop.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::LinkListener:
  // If the user selects the link we need to fire off the default browser that
  // by some convoluted logic should not be chrome.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Controls which version of the text to use.
  size_t version_;

  // We don't own any of these pointers. The |popup_| owns itself and owns the
  // other views.
  views::Widget* popup_;
  views::RadioButton* try_chrome_;
  views::RadioButton* kill_chrome_;
  Result result_;

  DISALLOW_COPY_AND_ASSIGN(TryChromeDialogView);
};

#endif  // CHROME_BROWSER_FIRST_RUN_TRY_CHROME_DIALOG_VIEW_H_
