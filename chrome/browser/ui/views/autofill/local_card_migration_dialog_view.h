// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/view_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Label;
class Separator;
}  // namespace views

namespace autofill {

class LocalCardMigrationDialogController;
class MigratableCardView;

class LocalCardMigrationDialogView : public LocalCardMigrationDialog,
                                     public views::ButtonListener,
                                     public views::StyledLabelListener,
                                     public views::DialogDelegateView,
                                     public views::WidgetObserver {
 public:
  LocalCardMigrationDialogView(LocalCardMigrationDialogController* controller,
                               content::WebContents* web_contents);
  ~LocalCardMigrationDialogView() override;

  // LocalCardMigrationDialog
  void ShowDialog() override;
  void CloseDialog() override;
  void OnMigrationFinished() override;

  // views::DialogDelegateView
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  void AddedToWidget() override;
  views::View* CreateExtraView() override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

 private:
  void Init();
  base::string16 GetDialogTitle() const;
  base::string16 GetDialogInstruction() const;
  int GetHeaderImageId() const;
  base::string16 GetOkButtonLabel() const;
  base::string16 GetCancelButtonLabel() const;
  void SetMigrationIsPending(bool is_pending);
  void ShowCloseButton();
  void UpdateDialogToPendingView();
  const std::vector<std::string> GetSelectedCardGuids();

  LocalCardMigrationDialogController* controller_;

  content::WebContents* web_contents_;

  views::Label* title_ = nullptr;

  views::Label* explanation_text_ = nullptr;

  // A list of MigratableCardView.
  views::View* card_list_view_ = nullptr;

  // Separates the card scroll bar view and the legal message.
  views::Separator* separator_ = nullptr;

  // The button displays "Close". If clicked, will close the dialog
  // in pending state.
  views::View* close_migration_dialog_button_ = nullptr;

  // The view that contains legal message and handles legal message links
  // clicking.
  LegalMessageView* legal_message_container_ = nullptr;

  // Timer that will call ShowCloseButton() after the migration process
  // has started and is pending for acertain amount of time which can be
  // configured through Finch.
  base::OneShotTimer show_close_button_timer_;

  // Whether the uploading is in progress and results are
  // pending.
  bool migration_pending_ = false;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_VIEW_H_
