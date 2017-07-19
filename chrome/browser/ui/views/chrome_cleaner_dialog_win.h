// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_DIALOG_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_DIALOG_WIN_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace safe_browsing {
class ChromeCleanerDialogController;
}

namespace views {
class Checkbox;
class LabelButton;
}  // namespace views

// A modal dialog asking the user if they want to remove harmful software from
// their computers by running the Chrome Cleanup tool.
//
// The strings and icons used in the dialog are provided by a
// |ChromeCleanerDialogController| object, which will also receive information
// about how the user interacts with the dialog. The controller object owns
// itself and will delete itself once it has received information about the
// user's interaction with the dialog. See the |ChromeCleanerDialogController|
// class's description for more details.
class ChromeCleanerDialog : public views::DialogDelegateView,
                            public views::ButtonListener {
 public:
  // The |controller| object manages its own lifetime and is not owned by
  // |ChromeCleanerDialog|. See the description of the
  // |ChromeCleanerDialogController| class for details.
  explicit ChromeCleanerDialog(
      safe_browsing::ChromeCleanerDialogController* controller);
  ~ChromeCleanerDialog() override;

  void Show(Browser* browser);

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;

  // views::DialogDelegate overrides.
  views::View* CreateFootnoteView() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  views::View* CreateExtraView() override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::View overrides.
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener overrides.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  Browser* browser_;
  // The pointer will be set to nullptr once the controller has been notified of
  // user interaction since the controller can delete itself after that point.
  safe_browsing::ChromeCleanerDialogController* controller_;
  views::LabelButton* details_button_;
  views::Checkbox* logs_permission_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_CLEANER_DIALOG_WIN_H_
