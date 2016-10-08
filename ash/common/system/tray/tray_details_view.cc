// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_details_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

class ScrollSeparator : public views::View {
 public:
  ScrollSeparator() {}

  ~ScrollSeparator() override {}

 private:
  // Overriden from views::View.
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(gfx::Rect(0, height() / 2, width(), 1), kBorderLightColor);
  }
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(1, kTrayPopupScrollSeparatorHeight);
  }

  DISALLOW_COPY_AND_ASSIGN(ScrollSeparator);
};

class ScrollBorder : public views::Border {
 public:
  ScrollBorder() {}
  ~ScrollBorder() override {}

  void set_visible(bool visible) { visible_ = visible; }

 private:
  // Overridden from views::Border.
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    if (!visible_)
      return;
    canvas->FillRect(gfx::Rect(0, view.height() - 1, view.width(), 1),
                     kBorderLightColor);
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(0, 0, 1, 0); }

  gfx::Size GetMinimumSize() const override { return gfx::Size(0, 1); }

  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(ScrollBorder);
};

TrayDetailsView::TrayDetailsView(SystemTrayItem* owner)
    : owner_(owner),
      title_row_(nullptr),
      scroller_(nullptr),
      scroll_content_(nullptr),
      scroll_border_(nullptr),
      back_button_(nullptr),
      settings_button_(nullptr) {
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
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    if (sender == back_button_) {
      TransitionToDefaultView();
      return;
    } else if (sender == settings_button_) {
      ShowSettings();
      return;
    }
  }

  HandleButtonPressed(sender, event);
}

void TrayDetailsView::CreateTitleRow(int string_id) {
  DCHECK(!title_row_);
  const int child_view_position =
      MaterialDesignController::IsSystemTrayMenuMaterial() ? 0 : child_count();
  title_row_ = new SpecialPopupRow();
  title_row_->SetTextLabel(string_id, this);

  AddChildViewAt(title_row_, child_view_position);

  CreateExtraTitleRowButtons();

  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    back_button_ = title_row_->AddBackButton(this);
    settings_button_ = title_row_->AddSettingsButton(this);
  }

  Layout();
}

void TrayDetailsView::CreateScrollableList() {
  DCHECK(!scroller_);
  scroll_content_ = new views::View;
  scroll_content_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  scroller_ = new FixedSizedScrollView;
  scroller_->SetContentsView(scroll_content_);

  // Note: |scroller_| takes ownership of |scroll_border_|.
  scroll_border_ = new ScrollBorder;
  scroller_->SetBorder(std::unique_ptr<views::Border>(scroll_border_));

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
  title_row_ = nullptr;
  scroller_ = nullptr;
  scroll_content_ = nullptr;
  back_button_ = nullptr;
  settings_button_ = nullptr;
}

void TrayDetailsView::HandleViewClicked(views::View* view) {}

void TrayDetailsView::HandleButtonPressed(views::Button* sender,
                                          const ui::Event& event) {}

void TrayDetailsView::CreateExtraTitleRowButtons() {}

void TrayDetailsView::ShowSettings() {
  // TODO(tdanderson): Store login status as a member in TrayDetailsView
  // instead of its derived classes. Use this to perform an early return
  // if launching WebUI settings is not permitted, and provide a default
  // implementation to ShowSettings().
}

void TrayDetailsView::TransitionToDefaultView() {
  // Cache pointer to owner in this function scope. TrayDetailsView will be
  // deleted after called ShowDefaultView.
  SystemTrayItem* owner = owner_;
  if (title_row_ && title_row_->content() && title_row_->content()->HasFocus())
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
