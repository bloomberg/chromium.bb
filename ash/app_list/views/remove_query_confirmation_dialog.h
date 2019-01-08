// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
#define ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_

#include "base/callback.h"
#include "ui/views/window/dialog_delegate.h"

namespace app_list {

// RemoveQueryConfirmationDialog displays the confirmation dialog for removing
// a recent query suggestion.
class RemoveQueryConfirmationDialog : public views::DialogDelegateView {
 public:
  // Callback to notify user's confirmation for removing the zero state
  // suggestion query. Invoked with true if user confirms removing query
  // suggestion; and false for declining the removal. The second parameter is
  // the event flags of user action for invoking the removal action on the
  // associated result.
  using RemovalConfirmationCallback = base::OnceCallback<void(bool, int)>;

  RemoveQueryConfirmationDialog(RemovalConfirmationCallback callback,
                                int event_flgas);
  ~RemoveQueryConfirmationDialog() override;

  // Shows the dialog with |parent| and |anchor_rect| in screen coordinates.
  void Show(gfx::NativeWindow parent, const gfx::Rect& anchor_rect);

 private:
  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;

  // views::DialogDelegate:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  RemovalConfirmationCallback confirm_callback_;
  int event_flags_;

  DISALLOW_COPY_AND_ASSIGN(RemoveQueryConfirmationDialog);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_REMOVE_QUERY_CONFIRMATION_DIALOG_H_
