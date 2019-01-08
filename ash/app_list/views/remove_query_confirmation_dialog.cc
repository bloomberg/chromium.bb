// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/remove_query_confirmation_dialog.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace app_list {

namespace {

constexpr int kDialogWidth = 320;
constexpr int kDialogYOffset = 32;

}  // namespace

RemoveQueryConfirmationDialog::RemoveQueryConfirmationDialog(
    RemovalConfirmationCallback confirm_callback,
    int event_flags)
    : confirm_callback_(std::move(confirm_callback)),
      event_flags_(event_flags) {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_DETAILS));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  AddChildView(label);
}

RemoveQueryConfirmationDialog::~RemoveQueryConfirmationDialog() {}

void RemoveQueryConfirmationDialog::Show(gfx::NativeWindow parent,
                                         const gfx::Rect& anchor_rect) {
  views::DialogDelegate::CreateDialogWidget(this, nullptr, parent);

  views::Widget* widget = GetWidget();
  DCHECK(widget);
  gfx::Rect widget_rect = widget->GetWindowBoundsInScreen();
  gfx::Point origin(anchor_rect.CenterPoint().x() - kDialogWidth / 2,
                    anchor_rect.y() + kDialogYOffset);
  widget_rect.set_origin(origin);
  widget->SetBounds(widget_rect);

  widget->Show();
}

base::string16 RemoveQueryConfirmationDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_TITLE);
}

ui::ModalType RemoveQueryConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool RemoveQueryConfirmationDialog::ShouldShowCloseButton() const {
  return false;
}

base::string16 RemoveQueryConfirmationDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_CANCEL
             ? l10n_util::GetStringUTF16(IDS_APP_CANCEL)
             : l10n_util::GetStringUTF16(IDS_REMOVE_SUGGESTION_BUTTON_LABEL);
}

bool RemoveQueryConfirmationDialog::Accept() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(true, event_flags_);

  return true;
}

bool RemoveQueryConfirmationDialog::Cancel() {
  if (confirm_callback_)
    std::move(confirm_callback_).Run(false, event_flags_);

  return true;
}

gfx::Size RemoveQueryConfirmationDialog::CalculatePreferredSize() const {
  const int default_width = kDialogWidth;
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

}  // namespace app_list
