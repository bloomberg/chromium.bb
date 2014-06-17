// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/views/app_list/app_list_dialog_contents_view.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_tab.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_tab.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
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
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        views::kButtonHEdgeMarginNew,
                                        views::kButtonVEdgeMarginNew,
                                        0));

  views::TabbedPane* tabbed_pane = new views::TabbedPane();
  AddChildView(tabbed_pane);

  tabbed_pane->AddTab(
      l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_SUMMARY_TAB_TITLE),
      new AppInfoSummaryTab(profile, app));
  tabbed_pane->AddTab(
      l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_PERMISSIONS_TAB_TITLE),
      new AppInfoPermissionsTab(profile, app));
  // TODO(sashab): Add the manage tab back once there is content for it.
}

AppInfoDialog::~AppInfoDialog() {}
