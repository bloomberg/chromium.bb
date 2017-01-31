// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webshare/webshare_target_picker_view.h"

#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

int kDialogWidth = 500;
int kDialogHeight = 400;

}  // namespace

namespace chrome {

void ShowWebShareTargetPickerDialog(
    gfx::NativeWindow parent_window,
    const std::vector<base::string16>& targets,
    const base::Callback<void(SharePickerResult)>& callback) {
  constrained_window::CreateBrowserModalDialogViews(
      new WebShareTargetPickerView(targets, callback), parent_window)
      ->Show();
}

}  // namespace chrome

WebShareTargetPickerView::WebShareTargetPickerView(
    const std::vector<base::string16>& targets,
    const base::Callback<void(SharePickerResult)>& close_callback)
    : targets_(targets), close_callback_(close_callback) {
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical, views::kPanelHorizMargin,
      views::kPanelVertMargin, views::kRelatedControlVerticalSpacing);
  SetLayoutManager(layout);

  views::Label* overview_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_WEBSHARE_TARGET_PICKER_LABEL));
  AddChildView(overview_label);

  std::vector<ui::TableColumn> table_columns{ui::TableColumn()};
  views::TableView* table =
      new views::TableView(this, table_columns, views::TEXT_ONLY, true);
  // Select the first row.
  if (RowCount() > 0)
    table->Select(0);

  // Create the table parent (a ScrollView which includes the scroll bars and
  // border). We add this parent (not the table itself) to the dialog.
  views::View* table_parent = table->CreateParentIfNecessary();
  AddChildView(table_parent);
  // Make the table expand to fill the space.
  layout->SetFlexForView(table_parent, 1);
}

WebShareTargetPickerView::~WebShareTargetPickerView() {}

gfx::Size WebShareTargetPickerView::GetPreferredSize() const {
  return gfx::Size(kDialogWidth, kDialogHeight);
}

ui::ModalType WebShareTargetPickerView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 WebShareTargetPickerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBSHARE_TARGET_PICKER_TITLE);
}

bool WebShareTargetPickerView::Cancel() {
  if (!close_callback_.is_null())
    close_callback_.Run(SharePickerResult::CANCEL);

  return true;
}

bool WebShareTargetPickerView::Accept() {
  if (!close_callback_.is_null())
    close_callback_.Run(SharePickerResult::SHARE);

  return true;
}

base::string16 WebShareTargetPickerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_WEBSHARE_TARGET_PICKER_COMMIT);

  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

int WebShareTargetPickerView::RowCount() {
  return targets_.size();
}

base::string16 WebShareTargetPickerView::GetText(int row, int /*column_id*/) {
  return targets_[row];
}

void WebShareTargetPickerView::SetObserver(ui::TableModelObserver* observer) {}
