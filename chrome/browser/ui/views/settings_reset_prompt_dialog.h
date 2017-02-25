// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace safe_browsing {
class SettingsResetPromptController;
}

namespace views {
class Link;
class View;
}

// A dialog intended for prompting users to reset some of their settings to
// their original default values. The dialog has two sections:
// 1. Main section with an explanation text
// 2. An expandable details section containing the details of the reset
//    operation.
class SettingsResetPromptDialog : public views::DialogDelegateView,
                                  public views::LinkListener {
 public:
  explicit SettingsResetPromptDialog(
      safe_browsing::SettingsResetPromptController* controller);
  ~SettingsResetPromptDialog() override;

  void Show(Browser* browser);

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;
  bool ShouldShowWindowIcon() const override;
  gfx::ImageSkia GetWindowIcon() override;
  base::string16 GetWindowTitle() const override;

  // ui::DialogModel overrides.
  bool ShouldDefaultButtonBeBlue() const override;

  // views::DialogDelegate overrides.
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  views::View* CreateExtraView() override;
  bool Accept() override;
  bool Cancel() override;

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;

  // views::LinkListener overrides.
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  class ExpandableMessageView;

  int ContentWidth() const;

  Browser* browser_;
  safe_browsing::SettingsResetPromptController* controller_;
  bool interaction_done_;

  ExpandableMessageView* details_view_;
  // The link that expands or hides the details section.
  views::Link* details_link_;

  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SETTINGS_RESET_PROMPT_DIALOG_H_
