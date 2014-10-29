// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/browser.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

class DownloadInProgressDialogView : public views::DialogDelegate {
 public:
  static void Show(gfx::NativeWindow parent_window,
                   int download_count,
                   Browser::DownloadClosePreventionType dialog_type,
                   bool app_modal,
                   const base::Callback<void(bool)>& callback);

 private:
  DownloadInProgressDialogView(int download_count,
                               Browser::DownloadClosePreventionType dialog_type,
                               bool app_modal,
                               const base::Callback<void(bool)>& callback);
  ~DownloadInProgressDialogView() override;

  // views::DialogDelegate:
  int GetDefaultDialogButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;

  const bool app_modal_;
  const base::Callback<void(bool)> callback_;
  views::MessageBoxView* message_box_view_;

  base::string16 title_text_;
  base::string16 ok_button_text_;
  base::string16 cancel_button_text_;

  gfx::Size dialog_dimensions_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
