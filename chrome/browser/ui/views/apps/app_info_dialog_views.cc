// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

void ShowChromeAppInfoDialog(gfx::NativeWindow parent_window,
                             Profile* profile,
                             const extensions::Extension* app,
                             const base::Closure& close_callback) {
  CreateBrowserModalDialogViews(new AppInfoView(profile, app, close_callback),
                                parent_window)->Show();
}

AppInfoView::AppInfoView(Profile* profile,
                         const extensions::Extension* app,
                         const base::Closure& close_callback)
    : profile_(profile),
      app_name_label(NULL),
      app_description_label(NULL),
      app_(app),
      close_callback_(close_callback) {
  // Create controls
  app_name_label = new views::Label(base::UTF8ToUTF16(app_->name()));
  app_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  app_description_label =
      new views::Label(base::UTF8ToUTF16(app_->description()));
  app_description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Layout controls
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  static const int kHeaderColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kHeaderColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::CENTER,
                        100.0f,
                        views::GridLayout::FIXED,
                        0,
                        0);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(app_name_label);

  layout->AddPaddingRow(0, views::kPanelSubVerticalSpacing);
  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(app_description_label);
}

AppInfoView::~AppInfoView() {}

bool AppInfoView::Cancel() {
  if (!close_callback_.is_null())
    close_callback_.Run();
  return true;
}

gfx::Size AppInfoView::GetPreferredSize() {
  static const int kDialogWidth = 360;
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

base::string16 AppInfoView::GetDialogButtonLabel(ui::DialogButton button)
    const {
  if (button == ui::DIALOG_BUTTON_CANCEL) {
    return l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_CLOSE_BUTTON);
  }
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

int AppInfoView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

bool AppInfoView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return true;
}

ui::ModalType AppInfoView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 AppInfoView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_TITLE);
}
