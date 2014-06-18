// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/views/app_list/app_list_dialog_contents_view.h"
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

void ShowAppInfoDialog(AppListControllerDelegate* app_list_controller_delegate,
                       Profile* profile,
                       const extensions::Extension* app) {
  gfx::NativeWindow app_list_window =
      app_list_controller_delegate->GetAppListWindow();
  DCHECK(app_list_window);
  gfx::Rect app_list_bounds = app_list_controller_delegate->GetAppListBounds();

  views::View* app_info_view = new AppInfoDialog(profile, app);
  views::Widget* dialog_widget = AppListDialogContentsView::CreateDialogWidget(
      app_list_window,
      app_list_bounds,
      new AppListDialogContentsView(app_list_controller_delegate,
                                    app_info_view));
  dialog_widget->Show();
}

AppInfoDialog::AppInfoDialog(Profile* profile,
                             const extensions::Extension* app) {
  // The width of this margin determines the spacing either side of the
  // horizontal separator underneath the summary panel.
  const int kHorizontalBorderSpacing = 1;
  const int kHorizontalSeparatorHeight = 2;
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kHorizontalBorderSpacing, 0, 0));
  AppInfoHeaderPanel* dialog_header = new AppInfoHeaderPanel(profile, app);
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
  dialog_body->AddChildView(new AppInfoSummaryPanel(profile, app));
  dialog_body->AddChildView(new AppInfoPermissionsPanel(profile, app));

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

AppInfoDialog::~AppInfoDialog() {
}
