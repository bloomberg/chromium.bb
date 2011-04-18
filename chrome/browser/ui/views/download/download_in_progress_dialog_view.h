// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

class Browser;

namespace gfx {
class Size;
}

namespace views {
class Label;
}

class DownloadInProgressDialogView : public views::View,
                                     public views::DialogDelegate {
 public:
  explicit DownloadInProgressDialogView(Browser* browser);
  virtual ~DownloadInProgressDialogView();

 private:
  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  Browser* browser_;
  views::Label* warning_;
  views::Label* explanation_;

  std::wstring ok_button_text_;
  std::wstring cancel_button_text_;

  string16 product_name_;

  gfx::Size dialog_dimensions_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
