// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

namespace views {
class MessageBoxView;
}

class DownloadInProgressDialogView : public views::DialogDelegate {
 public:
  static void Show(Browser* browser, gfx::NativeWindow parent_window);

 private:
  explicit DownloadInProgressDialogView(Browser* browser);
  virtual ~DownloadInProgressDialogView();

  // views::DialogDelegate:
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  Browser* browser_;
  views::MessageBoxView* message_box_view_;

  string16 title_text_;
  string16 ok_button_text_;
  string16 cancel_button_text_;

  gfx::Size dialog_dimensions_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
