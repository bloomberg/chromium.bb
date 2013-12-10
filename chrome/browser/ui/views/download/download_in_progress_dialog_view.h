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
  virtual ~DownloadInProgressDialogView();

  // views::DialogDelegate:
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

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
