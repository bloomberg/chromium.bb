// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/chooser_bubble_ui_view.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/origin.h"

namespace {

// Chooser permission bubble width
const int kChooserPermissionBubbleWidth = 300;

// Chooser permission bubble height
const int kChooserPermissionBubbleHeight = 200;

}  // namespace

std::unique_ptr<BubbleUi> ChooserBubbleController::BuildBubbleUi() {
  return base::WrapUnique(new ChooserBubbleUiView(browser_, this));
}

class ChooserTableModel;

///////////////////////////////////////////////////////////////////////////////
// View implementation for the chooser bubble.
class ChooserBubbleUiViewDelegate : public views::BubbleDialogDelegateView,
                                    public views::StyledLabelListener,
                                    public views::TableViewObserver {
 public:
  ChooserBubbleUiViewDelegate(views::View* anchor_view,
                              views::BubbleBorder::Arrow anchor_arrow,
                              ChooserBubbleController* controller);
  ~ChooserBubbleUiViewDelegate() override;

  // views::BubbleDialogDelegateView:
  bool ShouldShowWindowTitle() const override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* CreateFootnoteView() override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(views::View* anchor_view,
                    views::BubbleBorder::Arrow anchor_arrow);

  // Called by ChooserBubbleUiView's destructor. When ChooserBubbleUiView object
  // is destroyed, the |controller_| it passed to this class may not be used any
  // more since it may be destroyed too.
  void ControllerDestroying();

  ChooserTableModel* chooser_table_model() const;

 private:
  ChooserBubbleController* controller_;

  views::TableView* table_view_;
  ChooserTableModel* chooser_table_model_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiViewDelegate);
};

ui::TableColumn ChooserTableColumn(int id, const std::string& title) {
  ui::TableColumn column;
  column.id = id;
  column.title = base::ASCIIToUTF16(title.c_str());
  return column;
}

class ChooserTableModel : public ui::TableModel,
                          public ChooserBubbleController::Observer {
 public:
  explicit ChooserTableModel(ChooserBubbleController* controller);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // ChooserBubbleController::Observer:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;

  void Update();

 private:
  ui::TableModelObserver* observer_;
  ChooserBubbleController* controller_;
};

ChooserBubbleUiViewDelegate::ChooserBubbleUiViewDelegate(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow,
    ChooserBubbleController* controller)
    : views::BubbleDialogDelegateView(anchor_view, anchor_arrow),
      controller_(controller) {
  // ------------------------------------
  // | Chooser bubble title             |
  // | -------------------------------- |
  // | | option 0                     | |
  // | | option 1                     | |
  // | | option 2                     | |
  // | |                              | |
  // | |                              | |
  // | |                              | |
  // | -------------------------------- |
  // |           [ Connect ] [ Cancel ] |
  // |----------------------------------|
  // | Not seeing your device? Get help |
  // ------------------------------------

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  // Lay out the table view.
  layout->StartRow(0, 0);
  std::vector<ui::TableColumn> table_columns;
  table_columns.push_back(ChooserTableColumn(
      0, "" /* Empty string makes the column title invisible */));
  chooser_table_model_ = new ChooserTableModel(controller_);
  table_view_ = new views::TableView(chooser_table_model_, table_columns,
                                     views::TEXT_ONLY, true);
  table_view_->set_select_on_remove(false);
  chooser_table_model_->SetObserver(table_view_);
  table_view_->SetObserver(this);
  table_view_->SetEnabled(controller_->NumOptions() > 0);
  layout->AddView(table_view_->CreateParentIfNecessary(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  kChooserPermissionBubbleWidth,
                  kChooserPermissionBubbleHeight);
}

ChooserBubbleUiViewDelegate::~ChooserBubbleUiViewDelegate() {
  chooser_table_model_->SetObserver(nullptr);
}

bool ChooserBubbleUiViewDelegate::ShouldShowWindowTitle() const {
  return true;
}

base::string16 ChooserBubbleUiViewDelegate::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(
      IDS_CHOOSER_BUBBLE_PROMPT,
      base::ASCIIToUTF16(controller_->GetOrigin().Serialize()));
}

base::string16 ChooserBubbleUiViewDelegate::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_CHOOSER_BUBBLE_CONNECT_BUTTON_TEXT
                                       : IDS_CHOOSER_BUBBLE_CANCEL_BUTTON_TEXT);
}

bool ChooserBubbleUiViewDelegate::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK
             ? !table_view_->selection_model().empty()
             : true;
}

views::View* ChooserBubbleUiViewDelegate::CreateFootnoteView() {
  base::string16 link =
      l10n_util::GetStringUTF16(IDS_CHOOSER_BUBBLE_GET_HELP_LINK_TEXT);
  size_t offset;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_CHOOSER_BUBBLE_FOOTNOTE_TEXT, link, &offset);
  views::StyledLabel* label = new views::StyledLabel(text, this);
  label->AddStyleRange(gfx::Range(offset, offset + link.length()),
                       views::StyledLabel::RangeStyleInfo::CreateForLink());
  return label;
}

bool ChooserBubbleUiViewDelegate::Accept() {
  if (controller_)
    controller_->Select(table_view_->selection_model().active());
  return true;
}

bool ChooserBubbleUiViewDelegate::Cancel() {
  if (controller_)
    controller_->Cancel();
  return true;
}

bool ChooserBubbleUiViewDelegate::Close() {
  if (controller_)
    controller_->Close();
  return true;
}

void ChooserBubbleUiViewDelegate::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  controller_->OpenHelpCenterUrl();
}

void ChooserBubbleUiViewDelegate::OnSelectionChanged() {
  GetDialogClientView()->UpdateDialogButtons();
}

void ChooserBubbleUiViewDelegate::UpdateAnchor(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow) {
  if (GetAnchorView() == anchor_view && arrow() == anchor_arrow)
    return;

  set_arrow(anchor_arrow);

  // Reposition the bubble based on the updated arrow and view.
  SetAnchorView(anchor_view);
}

void ChooserBubbleUiViewDelegate::ControllerDestroying() {
  controller_ = nullptr;
}

ChooserTableModel* ChooserBubbleUiViewDelegate::chooser_table_model() const {
  return chooser_table_model_;
}

ChooserTableModel::ChooserTableModel(ChooserBubbleController* controller)
    : observer_(nullptr), controller_(controller) {
  controller_->set_observer(this);
}

int ChooserTableModel::RowCount() {
  // When there are no devices, the table contains a message saying there
  // are no devices, so the number of rows is always at least 1.
  return std::max(static_cast<int>(controller_->NumOptions()), 1);
}

base::string16 ChooserTableModel::GetText(int row, int column_id) {
  int num_options = static_cast<int>(controller_->NumOptions());
  if (num_options == 0) {
    DCHECK_EQ(0, row);
    return l10n_util::GetStringUTF16(
        IDS_CHOOSER_BUBBLE_NO_DEVICES_FOUND_PROMPT);
  }

  DCHECK_GE(row, 0);
  DCHECK_LT(row, num_options);
  return controller_->GetOption(static_cast<size_t>(row));
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

  if (controller_->NumOptions() == 0) {
    observer_->OnModelChanged();
    table_view->SetEnabled(false);
  } else {
    table_view->SetEnabled(true);
  }
}

//////////////////////////////////////////////////////////////////////////////
// ChooserBubbleUiView

ChooserBubbleUiView::ChooserBubbleUiView(Browser* browser,
                                         ChooserBubbleController* controller)
    : browser_(browser),
      controller_(controller),
      chooser_bubble_ui_view_delegate_(nullptr) {
  DCHECK(browser_);
  DCHECK(controller_);
}

ChooserBubbleUiView::~ChooserBubbleUiView() {
  if (chooser_bubble_ui_view_delegate_)
    chooser_bubble_ui_view_delegate_->ControllerDestroying();
}

void ChooserBubbleUiView::Show(BubbleReference /*bubble_reference*/) {
  chooser_bubble_ui_view_delegate_ = new ChooserBubbleUiViewDelegate(
      GetAnchorView(), GetAnchorArrow(), controller_);

  // Set |parent_window| because some valid anchors can become hidden.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  chooser_bubble_ui_view_delegate_->set_parent_window(widget->GetNativeView());

  views::BubbleDialogDelegateView::CreateBubble(
      chooser_bubble_ui_view_delegate_)
      ->Show();

  chooser_bubble_ui_view_delegate_->chooser_table_model()->Update();
}

void ChooserBubbleUiView::Close() {}

void ChooserBubbleUiView::UpdateAnchorPosition() {
  chooser_bubble_ui_view_delegate_->UpdateAnchor(GetAnchorView(),
                                                 GetAnchorArrow());
}

views::View* ChooserBubbleUiView::GetAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return browser_view->GetLocationBarView()->location_icon_view();

  if (browser_view->IsFullscreenBubbleVisible())
    return browser_view->exclusive_access_bubble()->GetView();

  return browser_view->top_container();
}

views::BubbleBorder::Arrow ChooserBubbleUiView::GetAnchorArrow() {
  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return views::BubbleBorder::TOP_LEFT;
  return views::BubbleBorder::NONE;
}
