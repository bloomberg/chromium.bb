// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_remoting_dialog_view.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace media_router {

// static
void MediaRemotingDialogView::ShowDialog(views::View* anchor_view) {
  DCHECK(!instance_);
  instance_ = new MediaRemotingDialogView(anchor_view);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

// static
void MediaRemotingDialogView::HideDialog() {
  if (IsShowing())
    instance_->GetWidget()->Close();
  // We also set |instance_| to nullptr in WindowClosing() which is called
  // asynchronously, because not all paths to close the dialog go through
  // HideDialog(). We set it here because IsShowing() should be false after
  // HideDialog() is called.
  instance_ = nullptr;
}

// static
bool MediaRemotingDialogView::IsShowing() {
  return instance_ != nullptr;
}

bool MediaRemotingDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 MediaRemotingDialogView::GetWindowTitle() const {
  return dialog_title_;
}

base::string16 MediaRemotingDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_OK
          ? IDS_MEDIA_ROUTER_REMOTING_DIALOG_OPTIMIZE_BUTTON
          : IDS_MEDIA_ROUTER_REMOTING_DIALOG_CANCEL_BUTTON);
}

int MediaRemotingDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool MediaRemotingDialogView::Accept() {
  // TODO(https://crbug.com/849020): Notify the Media Remoting feature that it
  // should be enabled.
  bool should_close_dialog = true;
  return should_close_dialog;
}

bool MediaRemotingDialogView::Cancel() {
  // TODO(https://crbug.com/849020): Notify the Media Remoting feature that it
  // shouldn't be enabled.
  bool should_close_dialog = true;
  return should_close_dialog;
}

bool MediaRemotingDialogView::Close() {
  return Cancel();
}

gfx::Size MediaRemotingDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

MediaRemotingDialogView::MediaRemotingDialogView(views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      dialog_title_(
          l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_REMOTING_DIALOG_TITLE)) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
}

MediaRemotingDialogView::~MediaRemotingDialogView() = default;

void MediaRemotingDialogView::Init() {
  views::Label* body_text = new views::Label(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_REMOTING_DIALOG_BODY_TEXT),
      views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT, views::style::STYLE_PRIMARY);
  body_text->SetMultiLine(true);
  body_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(body_text);

  remember_choice_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_REMOTING_DIALOG_CHECKBOX));
  AddChildView(remember_choice_checkbox_);
}

void MediaRemotingDialogView::WindowClosing() {
  if (instance_ == this)
    instance_ = nullptr;
}

// static
MediaRemotingDialogView* MediaRemotingDialogView::instance_ = nullptr;

}  // namespace media_router
