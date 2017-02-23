// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webshare/webshare_target_picker_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/gurl.h"

namespace {

int kDialogWidth = 500;
int kDialogHeight = 400;

}

// Supplies data to the table view.
class TargetPickerTableModel : public ui::TableModel {
 public:
  explicit TargetPickerTableModel(
      const std::vector<std::pair<base::string16, GURL>>* targets);

 private:
  // ui::TableModel overrides:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // Owned by WebShareTargetPickerView.
  const std::vector<std::pair<base::string16, GURL>>* targets_;

  DISALLOW_COPY_AND_ASSIGN(TargetPickerTableModel);
};

TargetPickerTableModel::TargetPickerTableModel(
    const std::vector<std::pair<base::string16, GURL>>* targets)
    : targets_(targets) {}

int TargetPickerTableModel::RowCount() {
  return targets_->size();
}

base::string16 TargetPickerTableModel::GetText(int row, int /*column_id*/) {
  // Show "title (origin)", to disambiguate titles that are the same, and as a
  // security measure.
  return (*targets_)[row].first +
         base::UTF8ToUTF16(" (" + (*targets_)[row].second.GetOrigin().spec() +
                           ")");
}

void TargetPickerTableModel::SetObserver(ui::TableModelObserver* observer) {}

namespace chrome {

void ShowWebShareTargetPickerDialog(
    gfx::NativeWindow parent_window,
    const std::vector<std::pair<base::string16, GURL>>& targets,
    const base::Callback<void(base::Optional<std::string>)>& callback) {
  constrained_window::CreateBrowserModalDialogViews(
      new WebShareTargetPickerView(targets, callback), parent_window)
      ->Show();
}

}  // namespace chrome

WebShareTargetPickerView::WebShareTargetPickerView(
    const std::vector<std::pair<base::string16, GURL>>& targets,
    const base::Callback<void(base::Optional<std::string>)>& close_callback)
    : targets_(targets),
      table_model_(base::MakeUnique<TargetPickerTableModel>(&targets_)),
      close_callback_(close_callback) {
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical,
      LayoutDelegate::Get()->GetMetric(
          LayoutDelegate::Metric::PANEL_CONTENT_MARGIN),
      views::kPanelVertMargin, views::kRelatedControlVerticalSpacing);
  SetLayoutManager(layout);

  views::Label* overview_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_WEBSHARE_TARGET_PICKER_LABEL));
  AddChildView(overview_label);

  std::vector<ui::TableColumn> table_columns{ui::TableColumn()};
  table_ = new views::TableView(table_model_.get(), table_columns,
                                views::TEXT_ONLY, true);
  // Select the first row.
  if (targets_.size() > 0)
    table_->Select(0);

  table_->set_observer(this);

  // Create the table parent (a ScrollView which includes the scroll bars and
  // border). We add this parent (not the table itself) to the dialog.
  views::View* table_parent = table_->CreateParentIfNecessary();
  AddChildView(table_parent);
  // Make the table expand to fill the space.
  layout->SetFlexForView(table_parent, 1);
}

WebShareTargetPickerView::~WebShareTargetPickerView() {
  // Clear the pointer from |table_| which currently points at |table_model_|.
  // Otherwise, |table_model_| will be deleted before |table_|, and |table_|'s
  // destructor will try to call a method on the model.
  table_->SetModel(nullptr);
}

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
    close_callback_.Run(base::nullopt);

  return true;
}

bool WebShareTargetPickerView::Accept() {
  if (!close_callback_.is_null()) {
    DCHECK(!table_->selection_model().empty());
    close_callback_.Run(targets_[table_->FirstSelectedRow()].second.spec());
  }

  return true;
}

base::string16 WebShareTargetPickerView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_WEBSHARE_TARGET_PICKER_COMMIT);

  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

bool WebShareTargetPickerView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  // User shouldn't select OK button if they haven't selected a target.
  if (button == ui::DIALOG_BUTTON_OK)
    return !table_->selection_model().empty();

  return true;
}

void WebShareTargetPickerView::OnSelectionChanged() {
  GetDialogClientView()->UpdateDialogButtons();
}

void WebShareTargetPickerView::OnDoubleClick() {
  GetDialogClientView()->AcceptWindow();
}
