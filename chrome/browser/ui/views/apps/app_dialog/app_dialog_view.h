// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_

#include "base/strings/string16.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class ImageSkia;
}

// The app dialog that may display the app's name, icon. This is the base class
// for app related dialog classes, e.g AppBlockDialogView, AppPauseDialogView.
class AppDialogView : public views::BubbleDialogDelegateView {
 public:
  explicit AppDialogView(const std::string& app_name,
                         const gfx::ImageSkia& image);
  ~AppDialogView() override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowIcon() const override;

  std::string app_name() const { return app_name_; }

 protected:
  void InitializeView(const base::string16& heading_text);

 private:
  const std::string app_name_;
  gfx::ImageSkia image_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_
