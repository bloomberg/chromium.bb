// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace views {
class Checkbox;
}  // namespace views

namespace media_router {

// Dialog that the user can use to either enable Media Remoting or keep it
// disabled during a Cast session. When Media Remoting is enabled, we send a
// media stream directly to the remote device instead of playing it locally and
// then mirroring it remotely.
class MediaRemotingDialogView : public views::BubbleDialogDelegateView {
 public:
  // Instantiates and shows the singleton dialog. The dialog must not be
  // currently shown.
  // TODO(https://crbug.com/849020): This method needs to take a callback to
  // call when the user interacts with the dialog.
  static void ShowDialog(views::View* anchor_view);

  // No-op if the dialog is currently not shown.
  static void HideDialog();

  static bool IsShowing();

  // views::WidgetDelegateView:
  bool ShouldShowCloseButton() const override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // ui::DialogModel:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  int GetDialogButtons() const override;

  // views::DialogDelegate:
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  explicit MediaRemotingDialogView(views::View* anchor_view);
  ~MediaRemotingDialogView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;

  // The singleton dialog instance. This is a nullptr when a dialog is not
  // shown.
  static MediaRemotingDialogView* instance_;

  // Title shown at the top of the dialog.
  base::string16 dialog_title_;

  // Checkbox the user can use to indicate whether the preference should be
  // sticky. If this is checked, we record the preference and don't show the
  // dialog again.
  views::Checkbox* remember_choice_checkbox_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaRemotingDialogView);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_REMOTING_DIALOG_VIEW_H_
