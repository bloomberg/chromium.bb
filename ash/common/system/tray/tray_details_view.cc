// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_details_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "base/containers/adapters.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {
namespace {

const int kHeaderRowSeparatorThickness = 1;
const SkColor kHeaderRowSeparatorColor = SkColorSetA(SK_ColorBLACK, 0x1F);

// A view that is used as ScrollView contents. It supports designating some of
// the children as sticky header rows. The sticky header rows are not scrolled
// above the top of the visible viewport until the next one "pushes" it up and
// are painted above other children. To indicate that a child is a sticky header
// row use set_id(kHeaderRowId).
class ScrollContentsView : public views::View,
                           public views::ViewTargeterDelegate {
 public:
  ScrollContentsView() {
    SetEventTargeter(base::MakeUnique<views::ViewTargeter>(this));
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  }
  ~ScrollContentsView() override {}

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    PositionHeaderRows();
  }

  void PaintChildren(const ui::PaintContext& context) override {
    for (int i = 0; i < child_count(); ++i) {
      if (child_at(i)->id() != kHeaderRowId && !child_at(i)->layer())
        child_at(i)->Paint(context);
    }
    // Paint header rows above other children in Z-order.
    for (auto& header : headers_) {
      if (!header.view()->layer())
        header.view()->Paint(context);
    }
  }

  void Layout() override {
    views::View::Layout();
    headers_.clear();
    for (int i = 0; i < child_count(); ++i) {
      views::View* view = child_at(i);
      if (view->id() == kHeaderRowId)
        headers_.emplace_back(view);
    }
    PositionHeaderRows();
  }

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (!details.is_add && details.parent == this) {
      headers_.erase(std::remove_if(headers_.begin(), headers_.end(),
                                    [details](const Header& header) {
                                      return header.view() == details.child;
                                    }),
                     headers_.end());
    }
  }

  // views::ViewTargeterDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override {
    // Give header rows first dibs on events.
    for (auto& header : headers_) {
      views::View* view = header.view();
      gfx::Rect local_to_header = rect;
      local_to_header.Offset(-view->x(), -view->y());
      if (ViewTargeterDelegate::DoesIntersectRect(view, local_to_header))
        return ViewTargeterDelegate::TargetForRect(view, local_to_header);
    }
    return ViewTargeterDelegate::TargetForRect(root, rect);
  }

 private:
  // A structure that keeps the original offset of each header between the
  // calls to Layout() to allow keeping track of which view should be sticky.
  class Header {
   public:
    explicit Header(views::View* header)
        : view_(header), offset_(header->y()), sticky_(true) {
      DecorateAsSticky(false);
    }

    // Sets decorations on a header row to indicate whether it is |sticky|.
    void DecorateAsSticky(bool sticky) {
      if (sticky_ == sticky)
        return;
      sticky_ = sticky;
      if (sticky) {
        view_->SetBorder(views::Border::CreateSolidSidedBorder(
            0, 0, kHeaderRowSeparatorThickness, 0, kHeaderRowSeparatorColor));
      } else {
        view_->SetBorder(views::Border::CreateSolidSidedBorder(
            kHeaderRowSeparatorThickness, 0, 0, 0, kHeaderRowSeparatorColor));
      }
    }

    const views::View* view() const { return view_; }
    views::View* view() { return view_; }
    int offset() const { return offset_; }

   private:
    // A header View that can be decorated as sticky.
    views::View* view_;

    // Offset from the top of ScrollContentsView to |view|'s original vertical
    // position.
    int offset_;

    // True if the header is decorated as sticky with a shadow below.
    bool sticky_;
  };

  // Adjusts y-position of header rows allowing one or two rows to stick to the
  // top of the visible viewport.
  void PositionHeaderRows() {
    const int scroll_offset = -y();
    Header* previous_header = nullptr;
    for (auto& header : base::Reversed(headers_)) {
      if (header.offset() >= scroll_offset) {
        header.DecorateAsSticky(false);
        previous_header = &header;
        continue;
      }
      views::View* header_view = header.view();
      if (previous_header &&
          previous_header->view()->y() <
              scroll_offset + header_view->height()) {
        // Lower header displacing the header above.
        header_view->SetY(previous_header->view()->y() - header_view->height());
        header.DecorateAsSticky(false);
        previous_header->DecorateAsSticky(false);
      } else {
        // A header becomes sticky.
        header_view->SetY(scroll_offset);
        header.DecorateAsSticky(true);
        header_view->Layout();
        header_view->SchedulePaint();
      }
      break;
    }
  }

  // Header child views that stick to the top of visible viewport when scrolled.
  std::vector<Header> headers_;

  DISALLOW_COPY_AND_ASSIGN(ScrollContentsView);
};

// Constants for the title row in material design.
const int kTitleRowVerticalPadding = 4;
const int kTitleRowSeparatorBorderHeight = 1;
const int kTitleRowProgressBarHeight = 2;
// The separator's height should be same as kTitleRowProgressBarHeight, and
// should not be larger than kTitleRowSeparatorBorderHeight.
const int kTitleRowSeparatorHeight = kTitleRowProgressBarHeight;
const int kTitleRowPaddingTop = kTitleRowVerticalPadding;
const int kTitleRowPaddingBottom =
    kTitleRowVerticalPadding - kTitleRowSeparatorHeight;
const SkColor kTitleRowSeparatorBorderColor = SkColorSetRGB(0xe0, 0xe0, 0xe0);

// Special layout to overlap the separator for title row and the progress bar
// which is displayed on it.
// Expected children are a views::Separator and, optionally, a
// views::ProgressBar. If the progress bar is present and visible, it is drawn
// over top of the separator so that only the progress bar is visible.
class TitleRowSeparatorLayout : public views::LayoutManager {
 public:
  TitleRowSeparatorLayout() {}
  ~TitleRowSeparatorLayout() override {}

  void Layout(views::View* host) override {
    int max_height = GetMaxHeight(host);
    for (int i = 0; i < host->child_count(); ++i) {
      views::View* child = host->child_at(i);
      gfx::Size child_size = child->GetPreferredSize();
      child->SetBounds(0, max_height - child_size.height(), host->width(),
                       child_size.height());
    }
  }

  gfx::Size GetPreferredSize(const views::View* host) const override {
    return gfx::Size(host->width(), GetMaxHeight(host));
  }

 private:
  int GetMaxHeight(const views::View* host) const {
    int max_height = 0;
    for (int i = 0; i < host->child_count(); ++i)
      max_height = std::max(max_height, host->child_at(i)->height());
    return max_height;
  }
};

class ScrollSeparator : public views::View {
 public:
  ScrollSeparator() {}

  ~ScrollSeparator() override {}

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(gfx::Rect(0, height() / 2, width(), 1), kBorderLightColor);
  }
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(1, kTrayPopupScrollSeparatorHeight);
  }

  DISALLOW_COPY_AND_ASSIGN(ScrollSeparator);
};

}  // namespace

class ScrollBorder : public views::Border {
 public:
  ScrollBorder() {}
  ~ScrollBorder() override {}

  void set_visible(bool visible) { visible_ = visible; }

 private:
  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    if (!visible_)
      return;
    canvas->FillRect(gfx::Rect(0, view.height() - 1, view.width(), 1),
                     kBorderLightColor);
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(0, 0, 1, 0); }

  gfx::Size GetMinimumSize() const override { return gfx::Size(0, 1); }

  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScrollBorder);
};

TrayDetailsView::TrayDetailsView(SystemTrayItem* owner)
    : owner_(owner),
      title_row_(nullptr),
      scroller_(nullptr),
      scroll_content_(nullptr),
      progress_bar_(nullptr),
      title_row_separator_(nullptr),
      scroll_border_(nullptr),
      back_button_(nullptr) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
}

TrayDetailsView::~TrayDetailsView() {}

void TrayDetailsView::OnViewClicked(views::View* sender) {
  if (!MaterialDesignController::IsSystemTrayMenuMaterial() && title_row_ &&
      sender == title_row_->content()) {
    TransitionToDefaultView();
    return;
  }

  HandleViewClicked(sender);
}

void TrayDetailsView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial() &&
      sender == back_button_) {
    TransitionToDefaultView();
    return;
  }

  HandleButtonPressed(sender, event);
}

void TrayDetailsView::CreateTitleRow(int string_id) {
  DCHECK(!title_row_);
  title_row_ = new SpecialPopupRow();
  title_row_->SetTextLabel(string_id, this);
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    title_row_->SetBorder(views::Border::CreateEmptyBorder(
        kTitleRowPaddingTop, 0, kTitleRowPaddingBottom, 0));
    AddChildViewAt(title_row_, 0);
    // In material design, we use a customized bottom border which is nomally a
    // simple separator (views::Separator) but can be combined with an
    // overlapping progress bar.
    title_row_separator_ = new views::View;
    title_row_separator_->SetLayoutManager(new TitleRowSeparatorLayout);
    views::Separator* separator =
        new views::Separator(views::Separator::HORIZONTAL);
    separator->SetColor(kTitleRowSeparatorBorderColor);
    separator->SetPreferredSize(kTitleRowSeparatorBorderHeight);
    separator->SetBorder(views::Border::CreateEmptyBorder(
        kTitleRowSeparatorHeight - kTitleRowSeparatorBorderHeight, 0, 0, 0));
    title_row_separator_->AddChildView(separator);
    AddChildViewAt(title_row_separator_, 1);
  } else {
    AddChildViewAt(title_row_, child_count());
  }

  CreateExtraTitleRowButtons();

  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    back_button_ = title_row_->AddBackButton(this);

  Layout();
}

void TrayDetailsView::CreateScrollableList() {
  DCHECK(!scroller_);
  scroll_content_ = new ScrollContentsView();
  scroller_ = new FixedSizedScrollView;
  scroller_->SetContentsView(scroll_content_);

  // Note: |scroller_| takes ownership of |scroll_border_|.
  scroll_border_ = new ScrollBorder;
  scroller_->SetBorder(std::unique_ptr<views::Border>(scroll_border_));

  AddChildView(scroller_);
}

void TrayDetailsView::CreateProgressBar() {
  DCHECK(!progress_bar_);
  DCHECK(title_row_separator_);
  progress_bar_ = new views::ProgressBar(kTitleRowProgressBarHeight);
  progress_bar_->SetVisible(false);
  title_row_separator_->AddChildView(progress_bar_);
}

void TrayDetailsView::AddScrollSeparator() {
  DCHECK(scroll_content_);
  // Do not draw the separator if it is the very first item
  // in the scrollable list.
  if (scroll_content_->has_children())
    scroll_content_->AddChildView(new ScrollSeparator);
}

void TrayDetailsView::Reset() {
  RemoveAllChildViews(true);
  title_row_ = nullptr;
  scroller_ = nullptr;
  scroll_content_ = nullptr;
  progress_bar_ = nullptr;
  title_row_separator_ = nullptr;
  back_button_ = nullptr;
}

void TrayDetailsView::HandleViewClicked(views::View* view) {}

void TrayDetailsView::HandleButtonPressed(views::Button* sender,
                                          const ui::Event& event) {}

void TrayDetailsView::CreateExtraTitleRowButtons() {}

void TrayDetailsView::TransitionToDefaultView() {
  // Cache pointer to owner in this function scope. TrayDetailsView will be
  // deleted after called ShowDefaultView.
  SystemTrayItem* owner = owner_;
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    if (back_button_ && back_button_->HasFocus())
      owner->set_restore_focus(true);
  } else {
    if (title_row_ && title_row_->content() &&
        title_row_->content()->HasFocus()) {
      owner->set_restore_focus(true);
    }
  }
  owner->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  owner->set_restore_focus(false);
}

void TrayDetailsView::Layout() {
  if (bounds().IsEmpty()) {
    views::View::Layout();
    return;
  }

  if (scroller_) {
    scroller_->set_fixed_size(gfx::Size());
    gfx::Size size = GetPreferredSize();

    // Set the scroller to fill the space above the bottom row, so that the
    // bottom row of the detailed view will always stay just above the title
    // row.
    gfx::Size scroller_size = scroll_content_->GetPreferredSize();
    scroller_->set_fixed_size(
        gfx::Size(width() + scroller_->GetScrollBarWidth(),
                  scroller_size.height() - (size.height() - height())));
  }

  views::View::Layout();

  if (title_row_ && !MaterialDesignController::IsSystemTrayMenuMaterial()) {
    // Always make sure the title row is bottom-aligned in non-MD.
    gfx::Rect fbounds = title_row_->bounds();
    fbounds.set_y(height() - title_row_->height());
    title_row_->SetBoundsRect(fbounds);
  }
}

void TrayDetailsView::OnPaintBorder(gfx::Canvas* canvas) {
  if (scroll_border_) {
    int index = GetIndexOf(scroller_);
    if (index < child_count() - 1 && child_at(index + 1) != title_row_)
      scroll_border_->set_visible(true);
    else
      scroll_border_->set_visible(false);
  }

  views::View::OnPaintBorder(canvas);
}

}  // namespace ash
