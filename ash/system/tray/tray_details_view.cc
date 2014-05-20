// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_details_view.h"

#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

class ScrollSeparator : public views::View {
 public:
  ScrollSeparator() {}

  virtual ~ScrollSeparator() {}

 private:
  // Overriden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(gfx::Rect(0, height() / 2, width(), 1), kBorderLightColor);
  }
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(1, kTrayPopupScrollSeparatorHeight);
  }

  DISALLOW_COPY_AND_ASSIGN(ScrollSeparator);
};

class ScrollBorder : public views::Border {
 public:
  ScrollBorder() {}
  virtual ~ScrollBorder() {}

  void set_visible(bool visible) { visible_ = visible; }

 private:
  // Overridden from views::Border.
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE {
    if (!visible_)
      return;
    canvas->FillRect(gfx::Rect(0, view.height() - 1, view.width(), 1),
                     kBorderLightColor);
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return gfx::Insets(0, 0, 1, 0);
  }

  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size(0, 1);
  }

  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(ScrollBorder);
};

TrayDetailsView::TrayDetailsView(SystemTrayItem* owner)
    : owner_(owner),
      footer_(NULL),
      scroller_(NULL),
      scroll_content_(NULL),
      scroll_border_(NULL) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
      0, 0, 0));
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
}

TrayDetailsView::~TrayDetailsView() {
}

void TrayDetailsView::CreateSpecialRow(int string_id,
                                       ViewClickListener* listener) {
  DCHECK(!footer_);
  footer_ = new SpecialPopupRow();
  footer_->SetTextLabel(string_id, listener);
  AddChildViewAt(footer_, child_count());
}

void TrayDetailsView::CreateScrollableList() {
  DCHECK(!scroller_);
  scroll_content_ = new views::View;
  scroll_content_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 1));
  scroller_ = new FixedSizedScrollView;
  scroller_->SetContentsView(scroll_content_);

  // Note: |scroller_| takes ownership of |scroll_border_|.
  scroll_border_ = new ScrollBorder;
  scroller_->SetBorder(scoped_ptr<views::Border>(scroll_border_));

  AddChildView(scroller_);
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
  footer_ = NULL;
  scroller_ = NULL;
  scroll_content_ = NULL;
}

void TrayDetailsView::TransitionToDefaultView() {
  // Cache pointer to owner in this function scope. TrayDetailsView will be
  // deleted after called ShowDefaultView.
  SystemTrayItem* owner = owner_;
  if (footer_ && footer_->content() && footer_->content()->HasFocus())
    owner->set_restore_focus(true);
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
    // bottom row of the detailed view will always stay just above the footer.
    gfx::Size scroller_size = scroll_content_->GetPreferredSize();
    scroller_->set_fixed_size(
        gfx::Size(width() + scroller_->GetScrollBarWidth(),
                  scroller_size.height() - (size.height() - height())));
  }

  views::View::Layout();

  if (footer_) {
    // Always make sure the footer element is bottom aligned.
    gfx::Rect fbounds = footer_->bounds();
    fbounds.set_y(height() - footer_->height());
    footer_->SetBoundsRect(fbounds);
  }
}

void TrayDetailsView::OnPaintBorder(gfx::Canvas* canvas) {
  if (scroll_border_) {
    int index = GetIndexOf(scroller_);
    if (index < child_count() - 1 && child_at(index + 1) != footer_)
      scroll_border_->set_visible(true);
    else
      scroll_border_->set_visible(false);
  }

  views::View::OnPaintBorder(canvas);
}

}  // namespace ash
