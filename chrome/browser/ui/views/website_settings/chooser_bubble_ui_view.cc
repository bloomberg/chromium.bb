// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/chooser_bubble_ui_view.h"

#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// Chooser permission bubble width
const int kChooserPermissionBubbleWidth = 300;

// Chooser permission bubble height
const int kChooserPermissionBubbleHeight = 200;

// Spacing constant for outer margin. This is added to the
// bubble margin itself to equalize the margins at 13px.
const int kBubbleOuterMargin = 5;

// Spacing between major items should be 9px.
const int kItemMajorSpacing = 9;

// Button border size, draws inside the spacing distance.
const int kButtonBorderSize = 2;

}  // namespace

scoped_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return make_scoped_ptr(new ChooserBubbleUiView(browser_, this));
}

class ChooserTableModel;

///////////////////////////////////////////////////////////////////////////////
// View implementation for the chooser bubble.
class ChooserBubbleUiViewDelegate : public views::BubbleDelegateView,
                                    public views::ButtonListener,
                                    public views::TableViewObserver {
 public:
  ChooserBubbleUiViewDelegate(views::View* anchor_view,
                              views::BubbleBorder::Arrow anchor_arrow,
                              ChooserBubbleUiView* owner,
                              ChooserBubbleDelegate* chooser_bubble_delegate);
  ~ChooserBubbleUiViewDelegate() override;

  void Close();

  // BubbleDelegateView:
  bool ShouldShowWindowTitle() const override;
  base::string16 GetWindowTitle() const override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // ButtonListener:
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(views::View* anchor_view,
                    views::BubbleBorder::Arrow anchor_arrow);

 private:
  friend ChooserBubbleUiView;

  ChooserBubbleUiView* owner_;
  ChooserBubbleDelegate* chooser_bubble_delegate_;

  views::LabelButton* connect_button_;
  views::LabelButton* cancel_button_;
  views::TableView* table_view_;
  ChooserTableModel* chooser_table_model_;
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiViewDelegate);
};

ui::TableColumn ChooserTableColumn(int id, const std::string& title) {
  ui::TableColumn column;
  column.id = id;
  column.title = base::ASCIIToUTF16(title.c_str());
  return column;
}

class ChooserTableModel : public ui::TableModel,
                          public ChooserBubbleDelegate::Observer {
 public:
  explicit ChooserTableModel(ChooserBubbleDelegate* chooser_bubble_delegate);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // ChooserBubbleDelegate::Observer:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;

  void Update();
  void SetConnectButton(views::LabelButton* connect_button);

 private:
  ui::TableModelObserver* observer_;
  ChooserBubbleDelegate* chooser_bubble_delegate_;
  views::LabelButton* connect_button_;
};

ChooserBubbleUiViewDelegate::ChooserBubbleUiViewDelegate(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow,
    ChooserBubbleUiView* owner,
    ChooserBubbleDelegate* chooser_bubble_delegate)
    : views::BubbleDelegateView(anchor_view, anchor_arrow),
      owner_(owner),
      chooser_bubble_delegate_(chooser_bubble_delegate),
      button_pressed_(false) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(1, 0);

  // Create a table view
  std::vector<ui::TableColumn> table_columns;
  table_columns.push_back(ChooserTableColumn(
      0, "" /* Empty string makes the column title invisible */));
  chooser_table_model_ = new ChooserTableModel(chooser_bubble_delegate_);
  table_view_ = new views::TableView(chooser_table_model_, table_columns,
                                     views::TEXT_ONLY, true);
  table_view_->set_select_on_remove(false);
  chooser_table_model_->SetObserver(table_view_);
  table_view_->SetObserver(this);
  layout->AddView(table_view_->CreateParentIfNecessary(), 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  kChooserPermissionBubbleWidth,
                  kChooserPermissionBubbleHeight);
  if (chooser_bubble_delegate_->NumOptions() == 0) {
    table_view_->SetEnabled(false);
  }

  layout->AddPaddingRow(0, kItemMajorSpacing);

  views::View* button_row = new views::View();
  views::GridLayout* button_layout = new views::GridLayout(button_row);
  views::ColumnSet* button_columns = button_layout->AddColumnSet(0);
  button_row->SetLayoutManager(button_layout);
  layout->StartRow(1, 0);
  layout->AddView(button_row);

  // Lay out the Connect/Cancel buttons.
  button_columns->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::FILL, 100,
                            views::GridLayout::USE_PREF, 0, 0);
  button_columns->AddPaddingColumn(0,
                                   kItemMajorSpacing - (2 * kButtonBorderSize));
  button_columns->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::FILL, 0,
                            views::GridLayout::USE_PREF, 0, 0);
  button_layout->StartRow(0, 0);

  base::string16 connect_text =
      l10n_util::GetStringUTF16(IDS_CHOOSER_BUBBLE_CONNECT_BUTTON_TEXT);
  connect_button_ = new views::LabelButton(this, connect_text);
  connect_button_->SetStyle(views::Button::STYLE_BUTTON);
  // Disable the connect button at the beginning since no device selected yet.
  connect_button_->SetEnabled(false);
  button_layout->AddView(connect_button_);
  chooser_table_model_->SetConnectButton(connect_button_);

  base::string16 cancel_text =
      l10n_util::GetStringUTF16(IDS_CHOOSER_BUBBLE_CANCEL_BUTTON_TEXT);
  cancel_button_ = new views::LabelButton(this, cancel_text);
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);
  button_layout->AddView(cancel_button_);

  button_layout->AddPaddingRow(0, kBubbleOuterMargin);
}

ChooserBubbleUiViewDelegate::~ChooserBubbleUiViewDelegate() {
  RemoveAllChildViews(true);
  if (owner_)
    owner_->Close();
  chooser_table_model_->SetObserver(nullptr);
}

void ChooserBubbleUiViewDelegate::Close() {
  if (!button_pressed_)
    chooser_bubble_delegate_->Close();
  owner_ = nullptr;
  GetWidget()->Close();
}

bool ChooserBubbleUiViewDelegate::ShouldShowWindowTitle() const {
  return true;
}

base::string16 ChooserBubbleUiViewDelegate::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CHOOSER_BUBBLE_PROMPT);
}

void ChooserBubbleUiViewDelegate::OnWidgetDestroying(views::Widget* widget) {
  views::BubbleDelegateView::OnWidgetDestroying(widget);
  if (owner_) {
    owner_->Close();
    owner_ = nullptr;
  }
}

void ChooserBubbleUiViewDelegate::ButtonPressed(views::Button* button,
                                                const ui::Event& event) {
  button_pressed_ = true;

  if (button == connect_button_)
    chooser_bubble_delegate_->Select(table_view_->selection_model().active());
  else
    chooser_bubble_delegate_->Cancel();

  if (owner_) {
    owner_->Close();
    owner_ = nullptr;
  }
}

void ChooserBubbleUiViewDelegate::OnSelectionChanged() {
  connect_button_->SetEnabled(!table_view_->selection_model().empty());
}

void ChooserBubbleUiViewDelegate::UpdateAnchor(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow) {
  if (GetAnchorView() == anchor_view && arrow() == anchor_arrow)
    return;

  set_arrow(anchor_arrow);

  // Update the border in the bubble: will either add or remove the arrow.
  views::BubbleFrameView* frame =
      views::BubbleDelegateView::GetBubbleFrameView();
  views::BubbleBorder::Arrow adjusted_arrow = anchor_arrow;
  if (base::i18n::IsRTL())
    adjusted_arrow = views::BubbleBorder::horizontal_mirror(adjusted_arrow);
  frame->SetBubbleBorder(scoped_ptr<views::BubbleBorder>(
      new views::BubbleBorder(adjusted_arrow, shadow(), color())));

  // Reposition the bubble based on the updated arrow and view.
  SetAnchorView(anchor_view);
}

ChooserTableModel::ChooserTableModel(
    ChooserBubbleDelegate* chooser_bubble_delegate)
    : observer_(nullptr), chooser_bubble_delegate_(chooser_bubble_delegate) {
  chooser_bubble_delegate_->set_observer(this);
}

int ChooserTableModel::RowCount() {
  // When there are no devices, the table contains a message saying there
  // are no devices, so the number of rows is always at least 1.
  return std::max(static_cast<int>(chooser_bubble_delegate_->NumOptions()), 1);
}

base::string16 ChooserTableModel::GetText(int row, int column_id) {
  int num_options = static_cast<int>(chooser_bubble_delegate_->NumOptions());
  if (num_options == 0) {
    DCHECK_EQ(0, row);
    return l10n_util::GetStringUTF16(
        IDS_CHOOSER_BUBBLE_NO_DEVICES_FOUND_PROMPT);
  }

  DCHECK_GE(row, 0);
  DCHECK_LT(row, num_options);
  return chooser_bubble_delegate_->GetOption(static_cast<size_t>(row));
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

  if (chooser_bubble_delegate_->NumOptions() == 0) {
    observer_->OnModelChanged();
    table_view->SetEnabled(false);
  } else {
    table_view->SetEnabled(true);
  }
}

void ChooserTableModel::SetConnectButton(views::LabelButton* connect_button) {
  connect_button_ = connect_button;
}

//////////////////////////////////////////////////////////////////////////////
// ChooserBubbleUiView

ChooserBubbleUiView::ChooserBubbleUiView(
    Browser* browser,
    ChooserBubbleDelegate* chooser_bubble_delegate)
    : browser_(browser),
      chooser_bubble_delegate_(chooser_bubble_delegate),
      chooser_bubble_ui_view_delegate_(nullptr) {
  DCHECK(browser_);
  DCHECK(chooser_bubble_delegate_);
}

ChooserBubbleUiView::~ChooserBubbleUiView() {}

void ChooserBubbleUiView::Show(BubbleReference bubble_reference) {
  chooser_bubble_ui_view_delegate_ = new ChooserBubbleUiViewDelegate(
      GetAnchorView(), GetAnchorArrow(), this, chooser_bubble_delegate_);

  // Set |parent_window| because some valid anchors can become hidden.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  chooser_bubble_ui_view_delegate_->set_parent_window(widget->GetNativeView());

  views::BubbleDelegateView::CreateBubble(chooser_bubble_ui_view_delegate_)
      ->Show();

  chooser_bubble_ui_view_delegate_->chooser_table_model_->Update();
}

void ChooserBubbleUiView::Close() {
  if (chooser_bubble_ui_view_delegate_) {
    chooser_bubble_ui_view_delegate_->Close();
    chooser_bubble_ui_view_delegate_ = nullptr;
  }
}

void ChooserBubbleUiView::UpdateAnchorPosition() {
  if (chooser_bubble_ui_view_delegate_) {
    chooser_bubble_ui_view_delegate_->UpdateAnchor(GetAnchorView(),
                                                   GetAnchorArrow());
  }
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
