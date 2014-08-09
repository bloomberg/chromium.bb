// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/idle_action_warning_dialog_view.h"

#include <algorithm>

#include "ash/shell.h"
#include "base/location.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace chromeos {

namespace {

const int kIdleActionWarningContentWidth = 300;

const int kCountdownUpdateIntervalMs = 1000;  // 1 second.

class FixedWidthLabel : public views::Label {
 public:
  explicit FixedWidthLabel(int width);
  virtual ~FixedWidthLabel();

  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 private:
  int width_;

  DISALLOW_COPY_AND_ASSIGN(FixedWidthLabel);
};

FixedWidthLabel::FixedWidthLabel(int width) : width_(width) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetMultiLine(true);
}

FixedWidthLabel::~FixedWidthLabel() {
}

gfx::Size FixedWidthLabel::GetPreferredSize() const {
  return gfx::Size(width_, GetHeightForWidth(width_));
}

}  // namespace

IdleActionWarningDialogView::IdleActionWarningDialogView(
    base::TimeTicks idle_action_time)
    : idle_action_time_(idle_action_time),
      label_(NULL) {
  label_ = new FixedWidthLabel(kIdleActionWarningContentWidth);
  label_->SetBorder(
      views::Border::CreateEmptyBorder(views::kPanelVertMargin,
                                       views::kButtonHEdgeMarginNew,
                                       views::kPanelVertMargin,
                                       views::kButtonHEdgeMarginNew));
  AddChildView(label_);
  SetLayoutManager(new views::FillLayout());

  UpdateLabel();

  views::DialogDelegate::CreateDialogWidget(
      this, ash::Shell::GetPrimaryRootWindow(), NULL)->Show();

  update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kCountdownUpdateIntervalMs),
      this,
      &IdleActionWarningDialogView::UpdateLabel);
}

void IdleActionWarningDialogView::CloseDialog() {
  update_timer_.Stop();
  GetDialogClientView()->CancelWindow();
}

void IdleActionWarningDialogView::Update(base::TimeTicks idle_action_time) {
  idle_action_time_ = idle_action_time;
  UpdateLabel();
}

ui::ModalType IdleActionWarningDialogView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 IdleActionWarningDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_IDLE_WARNING_TITLE);
}

int IdleActionWarningDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool IdleActionWarningDialogView::Cancel() {
  return !update_timer_.IsRunning();
}

IdleActionWarningDialogView::~IdleActionWarningDialogView() {
}

void IdleActionWarningDialogView::UpdateLabel() {
  const base::TimeDelta time_until_idle_action =
      std::max(idle_action_time_ - base::TimeTicks::Now(),
               base::TimeDelta());
  label_->SetText(l10n_util::GetStringFUTF16(
      IDS_IDLE_WARNING_LOGOUT_WARNING,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG,
                               10,
                               time_until_idle_action)));
}

}  // namespace chromeos
