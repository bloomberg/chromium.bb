// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_TRY_CHROME_DIALOG_VIEW_H_
#define CHROME_BROWSER_FIRST_RUN_TRY_CHROME_DIALOG_VIEW_H_

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
class Checkbox;
class Widget;
}

// This class displays a modal dialog using the views system. The dialog asks
// the user to give chrome another try. This class only handles the UI so the
// resulting actions are up to the caller. One flavor looks like this:
//
//   +-----------------------------------------------+
//   | |icon| There is a new, safer version      [x] |
//   | |icon| of Google Chrome available             |
//   |        [o] Try it out (already installed)     |
//   |        [ ] Uninstall Google Chrome            |
//   |        [ OK ] [Don't bug me]                  |
//   |        _why_am_I_seeing this?_                |
//   +-----------------------------------------------+
//
// Another flavor looks like:
//   +-----------------------------------------------+
//   | |icon| There is a new, safer version      [x] |
//   | |icon| of Google Chrome available             |
//   |        [o] Try it out (already installed)     |
//   |        [ ] Don't bug me                       |
//   |                  [ OK ]                       |
//   +-----------------------------------------------+
//
// And the 2013 version looks like:
//   +-----------------------------------------------+
//   | |icon| There is a new version of          [x] |
//   | |icon| Google Chrome available                |
//   |        [o] Try it out (already installed)     |
//   |        [ ] Don't bug me                       |
//   | --------------------------------------------- |
//   | [x] Make it the default browser       [ OK ]  |
//   +-----------------------------------------------+

class TryChromeDialogView : public views::ButtonListener,
                            public views::LinkListener {
 public:
  // Receives a handle to the active modal dialog, or NULL when the active
  // dialog is dismissed.
  typedef base::Callback<void(gfx::NativeWindow active_dialog)>
      ActiveModalDialogListener;

  enum Result {
    TRY_CHROME,             // Launch chrome right now.
    TRY_CHROME_AS_DEFAULT,  // Launch chrome and make it the default.
    NOT_NOW,                // Don't launch chrome. Exit now.
    UNINSTALL_CHROME,       // Initiate chrome uninstall and exit.
    DIALOG_ERROR,           // An error occurred creating the dialog.
    COUNT
  };

  // Shows a modal dialog asking the user to give chrome another try. See
  // above for the possible outcomes of the function. This is an experimental,
  // non-localized dialog.
  // |flavor| can be 0, 1, 2 or 3 and selects what strings to present.
  // |listener| will be notified when the dialog becomes active and when it is
  // dismissed.
  // Note that the dialog has no parent and it will position itself in a lower
  // corner of the screen. The dialog does not steal focus and does not have an
  // entry in the taskbar.
  static Result Show(size_t flavor, const ActiveModalDialogListener& listener);

 private:
  explicit TryChromeDialogView(size_t flavor);
  virtual ~TryChromeDialogView();

  Result ShowModal(const ActiveModalDialogListener& listener);

  // Returns a screen rectangle that is fit to show the window. In particular
  // it has the following properties: a) is visible and b) is attached to the
  // bottom of the working area. For LTR machines it returns a left side
  // rectangle and for RTL it returns a right side rectangle so that the dialog
  // does not compete with the standar place of the start menu.
  gfx::Rect ComputeWindowPosition(int width, int height, bool is_RTL);

  // Create a windows region that looks like a toast of width |w| and height
  // |h|. This is best effort, so we don't care much if the operation fails.
  void SetToastRegion(HWND window, int w, int h);

  // views::ButtonListener:
  // We have two buttons and according to what the user clicked we set |result_|
  // and we should always close and end the modal loop.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener:
  // If the user selects the link we need to fire off the default browser that
  // by some convoluted logic should not be chrome.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Controls which flavor of the heading text to use.
  size_t flavor_;

  // We don't own any of these pointers. The |popup_| owns itself and owns the
  // other views.
  views::Widget* popup_;
  views::RadioButton* try_chrome_;
  views::RadioButton* kill_chrome_;
  views::RadioButton* dont_try_chrome_;
  views::Checkbox* make_default_;
  Result result_;

  DISALLOW_COPY_AND_ASSIGN(TryChromeDialogView);
};

#endif  // CHROME_BROWSER_FIRST_RUN_TRY_CHROME_DIALOG_VIEW_H_
