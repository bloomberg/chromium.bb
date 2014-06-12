// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_panel.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

void ShowAppInfoDialog(gfx::NativeWindow parent_window,
                       const gfx::Rect& dialog_widget_bounds,
                       Profile* profile,
                       const extensions::Extension* app,
                       const base::Closure& close_callback) {
  views::Widget* dialog = CreateBrowserModalDialogViews(
      new AppInfoDialog(parent_window, profile, app, close_callback),
      parent_window);
  dialog->SetBounds(dialog_widget_bounds);
  dialog->Show();
}

AppInfoDialog::AppInfoDialog(gfx::NativeWindow parent_window,
                             Profile* profile,
                             const extensions::Extension* app,
                             const base::Closure& close_callback)
    : parent_window_(parent_window),
      profile_(profile),
      app_(app),
      close_callback_(close_callback) {
  // The width of this margin determines the spacing either side of the
  // horizontal separator underneath the summary panel.
  const int kHorizontalBorderSpacing = 1;
  const int kHorizontalSeparatorHeight = 2;
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kHorizontalBorderSpacing, 0, 0));
  AppInfoHeaderPanel* dialog_header =
      new AppInfoHeaderPanel(parent_window_, profile_, app_, close_callback_);
  dialog_header->SetBorder(views::Border::CreateSolidSidedBorder(
      0, 0, kHorizontalSeparatorHeight, 0, SK_ColorLTGRAY));

  // Make a vertically stacked view of all the panels we want to display in the
  // dialog.
  views::View* dialog_body = new views::View();
  dialog_body->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           views::kButtonHEdgeMarginNew,
                           views::kPanelVertMargin,
                           views::kUnrelatedControlVerticalSpacing));
  dialog_body->AddChildView(
      new AppInfoSummaryPanel(parent_window_, profile_, app_, close_callback_));
  dialog_body->AddChildView(new AppInfoPermissionsPanel(
      parent_window_, profile_, app_, close_callback_));

  // Clip the scrollable view so that the scrollbar appears. As long as this
  // is larger than the height of the dialog, it will be resized to the dialog's
  // actual height.
  // TODO(sashab): Add ClipHeight() as a parameter-less method to
  // views::ScrollView(), which mimics this behaviour.
  const int kMaxDialogHeight = 1000;
  views::ScrollView* dialog_body_scrollview = new views::ScrollView();
  dialog_body_scrollview->ClipHeightTo(kMaxDialogHeight, kMaxDialogHeight);
  dialog_body_scrollview->SetContents(dialog_body);

  AddChildView(dialog_header);
  AddChildView(dialog_body_scrollview);
}

AppInfoDialog::~AppInfoDialog() {}

bool AppInfoDialog::Cancel() {
  if (!close_callback_.is_null())
    close_callback_.Run();
  return true;
}

gfx::Size AppInfoDialog::GetPreferredSize() const {
  // These numbers represent the size of the view, not the total size of the
  // dialog. The actual dialog will be slightly taller (have a larger height)
  // than what is specified here.
  static const int kDialogWidth = 360;
  static const int kDialogHeight = 360;
  return gfx::Size(kDialogWidth, kDialogHeight);
}

int AppInfoDialog::GetDialogButtons() const { return ui::DIALOG_BUTTON_NONE; }

ui::ModalType AppInfoDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}
