// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/installer/util/experiment_storage.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Widget;
}

// This class displays a modal dialog using the views system. The dialog asks
// the user to give Chrome another try. This class only handles the UI so the
// resulting actions are up to the caller.
//
// The layout is as follows:
//
//   +-----------------------------------------------+
//   | |icon| Header text.                       [x] |
//   |                                               |
//   |        Body text.                             |
//   |        [ Open Chrome ] [No Thanks]            |
//   +-----------------------------------------------+
//
// Some variants do not have body text, or only have one button.
class TryChromeDialog : public views::ButtonListener {
 public:
  // Receives a handle to the active modal dialog, or NULL when the active
  // dialog is dismissed.
  typedef base::Callback<void(gfx::NativeWindow active_dialog)>
      ActiveModalDialogListener;

  enum Result {
    NOT_NOW,                    // Don't launch chrome. Exit now.
    OPEN_CHROME_WELCOME,        // Launch Chrome to the standard Welcome page.
    OPEN_CHROME_WELCOME_WIN10,  // Launch Chrome to the Win10 Welcome page.
    OPEN_CHROME_DEFAULT,        // Launch Chrome to the default page.
  };

  // Shows a modal dialog asking the user to give Chrome another try. See
  // above for the possible outcomes of the function.
  // |group| selects what strings to present and what controls are shown.
  // |listener| will be notified when the dialog becomes active and when it is
  // dismissed.
  // Note that the dialog has no parent and it will position itself in a lower
  // corner of the screen or near the Chrome taskbar button.
  // The dialog does not steal focus and does not have an entry in the taskbar.
  static Result Show(size_t group, const ActiveModalDialogListener& listener);

 private:
  // Indicates whether the dialog is modal
  enum class DialogType {
    MODAL,              // Modal dialog.
    MODELESS_FOR_TEST,  // Modeless dialog.
  };

  // Indicates the usage type. Chrome or tests.
  enum class UsageType {
    FOR_CHROME,
    FOR_TESTING,
  };

  friend class TryChromeDialogTest;

  // Creates a Try Chrome toast dialog. |group| signifies an experiment group
  // which dictactes messaging text and presence of ui elements.
  explicit TryChromeDialog(size_t group);
  ~TryChromeDialog() override;

  // Helper function to show the dialog.
  // The |dialog_type| parameter indicates whether the dialog is modal.
  // Note that modeless invocation returns before the user has made a
  // selection, and is used in testing. This case will always return NOT_NOW.
  // The |usage_type| parameter indicates whether this is being invoked by
  // Chrome or a test.
  Result ShowDialog(const ActiveModalDialogListener& listener,
                    DialogType dialog_type,
                    UsageType usage_type);

  // Returns a screen rectangle that is fit to show the window. In particular
  // it has the following properties: a) is visible and b) is attached to the
  // bottom of the working area. For LTR machines it returns a left side
  // rectangle and for RTL it returns a right side rectangle so that the dialog
  // does not compete with the standard place of the start menu.
  gfx::Rect ComputePopupBounds(const gfx::Size& size, bool is_RTL);

  // views::ButtonListener:
  // We have two buttons and according to what the user clicked we set |result_|
  // and we should always close and end the modal loop.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Controls whether we're running in testing config.
  // Experiment metrics setting is disabled in tests.
  UsageType usage_type_;

  // Controls which experiment group to use for varying the layout and controls.
  const size_t group_;

  // Time when the toast was displayed.
  base::TimeTicks time_shown_;

  // Unowned; |popup_| owns itself.
  views::Widget* popup_;

  // RunLoop to run the dialog before the main message loop.
  std::unique_ptr<base::RunLoop> run_loop_;

  // Experiment feedback interface.
  installer::ExperimentStorage storage_;

  // Result of displaying the dialog: accepted, dismissed, etc.
  Result result_;

  DISALLOW_COPY_AND_ASSIGN(TryChromeDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRY_CHROME_DIALOG_H_
