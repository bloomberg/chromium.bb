// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/views/app_list/app_list_dialog_contents_view.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_panel.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

void ShowAppInfoDialog(AppListControllerDelegate* app_list_controller_delegate,
                       Profile* profile,
                       const extensions::Extension* app) {
  gfx::NativeWindow app_list_window =
      app_list_controller_delegate->GetAppListWindow();
  DCHECK(app_list_window);
  gfx::Rect app_list_bounds = app_list_controller_delegate->GetAppListBounds();

  views::View* app_info_view = new AppInfoDialog(app_list_window, profile, app);
  views::Widget* dialog_widget = AppListDialogContentsView::CreateDialogWidget(
      app_list_window,
      app_list_bounds,
      new AppListDialogContentsView(app_list_controller_delegate,
                                    app_info_view));
  dialog_widget->Show();
}

AppInfoDialog::AppInfoDialog(gfx::NativeWindow parent_window,
                             Profile* profile,
                             const extensions::Extension* app)
    : dialog_header_(NULL), dialog_body_(NULL), dialog_footer_(NULL) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Create one column that fills the whole dialog.
  int kColumnSetId = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,  // Stretch the column to the width of the dialog.
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  const int kHorizontalSeparatorHeight = 1;
  dialog_header_ = new AppInfoHeaderPanel(profile, app);
  dialog_header_->SetBorder(views::Border::CreateSolidSidedBorder(
      0, 0, kHorizontalSeparatorHeight, 0, app_list::kDialogSeparatorColor));

  dialog_footer_ = new AppInfoFooterPanel(parent_window, profile, app);
  dialog_footer_->SetBorder(views::Border::CreateSolidSidedBorder(
      kHorizontalSeparatorHeight, 0, 0, 0, app_list::kDialogSeparatorColor));
  if (!dialog_footer_->has_children()) {
    // If there are no controls in the footer, don't add it to the dialog.
    delete dialog_footer_;
    dialog_footer_ = NULL;
  }

  // Make a vertically stacked view of all the panels we want to display in the
  // dialog.
  views::View* dialog_body_contents = new views::View();
  dialog_body_contents->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           views::kButtonHEdgeMarginNew,
                           views::kPanelVertMargin,
                           views::kUnrelatedControlVerticalSpacing));
  dialog_body_contents->AddChildView(new AppInfoSummaryPanel(profile, app));
  dialog_body_contents->AddChildView(new AppInfoPermissionsPanel(profile, app));

  // Clip the scrollable view so that the scrollbar appears. As long as this
  // is larger than the height of the dialog, it will be resized to the dialog's
  // actual height.
  // TODO(sashab): Add ClipHeight() as a parameter-less method to
  // views::ScrollView() to mimic this behaviour.
  const int kMaxDialogHeight = 1000;
  dialog_body_ = new views::ScrollView();
  dialog_body_->ClipHeightTo(kMaxDialogHeight, kMaxDialogHeight);
  dialog_body_->SetContents(dialog_body_contents);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(dialog_header_);

  layout->StartRow(1, kColumnSetId);
  layout->AddView(dialog_body_);

  if (dialog_footer_) {
    layout->StartRow(0, kColumnSetId);
    layout->AddView(dialog_footer_);
  }
}

AppInfoDialog::~AppInfoDialog() {
}
