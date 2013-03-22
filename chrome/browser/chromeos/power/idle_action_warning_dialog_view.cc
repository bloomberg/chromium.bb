// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/idle_action_warning_dialog_view.h"

#include "ash/shell.h"
#include "grit/generated_resources.h"
#include "ui/aura/root_window.h"
#include "ui/base/l10n/l10n_util.h"
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

class FixedWidthLabel : public views::Label {
 public:
  FixedWidthLabel(const string16& text, int width);
  virtual ~FixedWidthLabel();

  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  int width_;

  DISALLOW_COPY_AND_ASSIGN(FixedWidthLabel);
};

FixedWidthLabel::FixedWidthLabel(const string16& text, int width)
    : Label(text),
      width_(width) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetMultiLine(true);
}

FixedWidthLabel::~FixedWidthLabel() {
}

gfx::Size FixedWidthLabel::GetPreferredSize() {
  return gfx::Size(width_, GetHeightForWidth(width_));
}

}  // namespace

IdleActionWarningDialogView::IdleActionWarningDialogView() : closing_(false) {
  FixedWidthLabel* content = new FixedWidthLabel(
        l10n_util::GetStringUTF16(IDS_IDLE_WARNING_LOGOUT_WARNING),
        kIdleActionWarningContentWidth);
  content->set_border(views::Border::CreateEmptyBorder(
      views::kPanelVertMargin, views::kPanelHorizMargin,
      views::kPanelVertMargin, views::kPanelHorizMargin));
  AddChildView(content);
  SetLayoutManager(new views::FillLayout());

  views::Widget::CreateWindowWithContext(this,
                                         ash::Shell::GetPrimaryRootWindow());
  GetWidget()->Show();
}

void IdleActionWarningDialogView::Close() {
  closing_ = true;
  GetDialogClientView()->CancelWindow();
}

ui::ModalType IdleActionWarningDialogView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 IdleActionWarningDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_IDLE_WARNING_TITLE);
}

int IdleActionWarningDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool IdleActionWarningDialogView::Cancel() {
  return closing_;
}

IdleActionWarningDialogView::~IdleActionWarningDialogView() {
}

}  // namespace chromeos
