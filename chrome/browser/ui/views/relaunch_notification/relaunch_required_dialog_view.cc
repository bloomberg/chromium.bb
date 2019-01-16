// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_required_dialog_view.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// static
views::Widget* RelaunchRequiredDialogView::Show(
    Browser* browser,
    base::Time deadline,
    base::RepeatingClosure on_accept) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      new RelaunchRequiredDialogView(deadline, std::move(on_accept)),
      browser->window()->GetNativeWindow());
  widget->Show();
  return widget;
}

RelaunchRequiredDialogView::~RelaunchRequiredDialogView() = default;

// static
RelaunchRequiredDialogView* RelaunchRequiredDialogView::FromWidget(
    views::Widget* widget) {
  return static_cast<RelaunchRequiredDialogView*>(
      widget->widget_delegate()->AsDialogDelegate());
}

void RelaunchRequiredDialogView::SetDeadline(base::Time deadline) {
  relaunch_required_timer_.SetDeadline(deadline);
}

bool RelaunchRequiredDialogView::Cancel() {
  base::RecordAction(base::UserMetricsAction("RelaunchRequired_Close"));

  return true;
}

bool RelaunchRequiredDialogView::Accept() {
  base::RecordAction(base::UserMetricsAction("RelaunchRequired_Accept"));

  on_accept_.Run();

  // Keep the dialog open in case shutdown is prevented for some reason so that
  // the user can try again if needed.
  return false;
}

int RelaunchRequiredDialogView::GetDefaultDialogButton() const {
  // Do not focus either button so that the user doesn't relaunch or dismiss by
  // accident if typing when the dialog appears.
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 RelaunchRequiredDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_RELAUNCH_ACCEPT_BUTTON
                                       : IDS_RELAUNCH_REQUIRED_CANCEL_BUTTON);
}

ui::ModalType RelaunchRequiredDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 RelaunchRequiredDialogView::GetWindowTitle() const {
  return relaunch_required_timer_.GetWindowTitle();
}

bool RelaunchRequiredDialogView::ShouldShowCloseButton() const {
  return false;
}

gfx::ImageSkia RelaunchRequiredDialogView::GetWindowIcon() {
  return gfx::CreateVectorIcon(gfx::IconDescription(
      vector_icons::kBusinessIcon, kTitleIconSize, gfx::kChromeIconGrey,
      base::TimeDelta(), gfx::kNoneIcon));
}

bool RelaunchRequiredDialogView::ShouldShowWindowIcon() const {
  return true;
}

int RelaunchRequiredDialogView::GetHeightForWidth(int width) const {
  const gfx::Insets insets = GetInsets();
  return body_label_->GetHeightForWidth(width - insets.width()) +
         insets.height();
}

void RelaunchRequiredDialogView::Layout() {
  body_label_->SetBoundsRect(GetContentsBounds());
}

gfx::Size RelaunchRequiredDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

// |relaunch_required_timer_| automatically starts for the next time the title
// needs to be updated (e.g., from "2 days" to "3 days").
RelaunchRequiredDialogView::RelaunchRequiredDialogView(
    base::Time deadline,
    base::RepeatingClosure on_accept)
    : on_accept_(on_accept),
      body_label_(nullptr),
      relaunch_required_timer_(
          deadline,
          base::BindRepeating(&RelaunchRequiredDialogView::UpdateWindowTitle,
                              base::Unretained(this))) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::RELAUNCH_REQUIRED);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

  body_label_ =
      new views::Label(l10n_util::GetStringUTF16(IDS_RELAUNCH_REQUIRED_BODY),
                       views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
  body_label_->SetMultiLine(true);
  body_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Align the body label with the left edge of the dialog's title.
  // TODO(bsep): Remove this when fixing https://crbug.com/810970.
  int title_offset = 2 * views::LayoutProvider::Get()
                             ->GetInsetsMetric(views::INSETS_DIALOG_TITLE)
                             .left() +
                     kTitleIconSize;
  body_label_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, title_offset - margins().left(), 0, 0)));

  AddChildView(body_label_);

  base::RecordAction(base::UserMetricsAction("RelaunchRequiredShown"));
}

void RelaunchRequiredDialogView::UpdateWindowTitle() {
  GetWidget()->UpdateWindowTitle();
}
