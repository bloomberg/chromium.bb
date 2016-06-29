// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chooser_content_view.h"

#include "chrome/grit/generated_resources.h"
#include "components/chooser_controller/chooser_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

const int kChooserWidth = 300;

const int kChooserHeight = 200;

}  // namespace

class ChooserTableModel : public ui::TableModel,
                          public ChooserController::Observer {
 public:
  explicit ChooserTableModel(ChooserController* chooser_controller);
  ~ChooserTableModel() override;

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // ChooserController::Observer:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;

  void Update();
  void ChooserControllerDestroying();

 private:
  ui::TableModelObserver* observer_;
  ChooserController* chooser_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChooserTableModel);
};

ChooserTableModel::ChooserTableModel(ChooserController* chooser_controller)
    : observer_(nullptr), chooser_controller_(chooser_controller) {
  chooser_controller_->set_observer(this);
}

ChooserTableModel::~ChooserTableModel() {
  chooser_controller_->set_observer(nullptr);
}

int ChooserTableModel::RowCount() {
  // When there are no devices, the table contains a message saying there
  // are no devices, so the number of rows is always at least 1.
  return std::max(static_cast<int>(chooser_controller_->NumOptions()), 1);
}

base::string16 ChooserTableModel::GetText(int row, int column_id) {
  int num_options = static_cast<int>(chooser_controller_->NumOptions());
  if (num_options == 0) {
    DCHECK_EQ(0, row);
    return l10n_util::GetStringUTF16(
        IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
  }

  DCHECK_GE(row, 0);
  DCHECK_LT(row, num_options);
  return chooser_controller_->GetOption(static_cast<size_t>(row));
}

void ChooserTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

void ChooserTableModel::OnOptionsInitialized() {
  if (observer_) {
    observer_->OnModelChanged();
    Update();
  }
}

void ChooserTableModel::OnOptionAdded(size_t index) {
  if (observer_) {
    observer_->OnItemsAdded(static_cast<int>(index), 1);
    Update();
  }
}

void ChooserTableModel::OnOptionRemoved(size_t index) {
  if (observer_) {
    observer_->OnItemsRemoved(static_cast<int>(index), 1);
    Update();
  }
}

void ChooserTableModel::Update() {
  views::TableView* table_view = static_cast<views::TableView*>(observer_);

  if (chooser_controller_->NumOptions() == 0) {
    observer_->OnModelChanged();
    table_view->SetEnabled(false);
  } else {
    table_view->SetEnabled(true);
  }
}

ChooserContentView::ChooserContentView(
    views::TableViewObserver* observer,
    std::unique_ptr<ChooserController> chooser_controller)
    : chooser_controller_(std::move(chooser_controller)), table_view_(nullptr) {
  std::vector<ui::TableColumn> table_columns;
  table_columns.push_back(ui::TableColumn());
  chooser_table_model_.reset(new ChooserTableModel(chooser_controller_.get()));
  table_view_ = new views::TableView(chooser_table_model_.get(), table_columns,
                                     views::TEXT_ONLY, true);
  table_view_->set_select_on_remove(false);
  chooser_table_model_->SetObserver(table_view_);
  table_view_->SetObserver(observer);
  table_view_->SetEnabled(chooser_controller_->NumOptions() > 0);

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, views::kRelatedControlVerticalSpacing);
  SetLayoutManager(layout);
  views::View* table_parent = table_view_->CreateParentIfNecessary();
  AddChildView(table_parent);
  layout->SetFlexForView(table_parent, 1);
}

ChooserContentView::~ChooserContentView() {
  table_view_->SetModel(nullptr);
  chooser_table_model_->SetObserver(nullptr);
}

gfx::Size ChooserContentView::GetPreferredSize() const {
  return gfx::Size(kChooserWidth, kChooserHeight);
}

base::string16 ChooserContentView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT
                                       : IDS_DEVICE_CHOOSER_CANCEL_BUTTON_TEXT);
}

bool ChooserContentView::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK ||
         !table_view_->selection_model().empty();
}

views::StyledLabel* ChooserContentView::CreateFootnoteView(
    views::StyledLabelListener* listener) const {
  base::string16 link =
      l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_GET_HELP_LINK_TEXT);
  size_t offset = 0;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_DEVICE_CHOOSER_FOOTNOTE_TEXT, link, &offset);
  views::StyledLabel* styled_label = new views::StyledLabel(text, listener);
  styled_label->AddStyleRange(
      gfx::Range(offset, offset + link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  return styled_label;
}

void ChooserContentView::Accept() {
  chooser_controller_->Select(table_view_->selection_model().active());
}

void ChooserContentView::Cancel() {
  chooser_controller_->Cancel();
}

void ChooserContentView::Close() {
  chooser_controller_->Close();
}

void ChooserContentView::StyledLabelLinkClicked() {
  chooser_controller_->OpenHelpCenterUrl();
}

void ChooserContentView::UpdateTableModel() {
  chooser_table_model_->Update();
}
