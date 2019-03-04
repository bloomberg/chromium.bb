// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_view.h"

#include <algorithm>
#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/new_desk_button.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

namespace {

constexpr int kBarHeight = 104;

base::string16 GetMiniViewTitle(int mini_view_index) {
  DCHECK_GE(mini_view_index, 0);
  DCHECK_LT(mini_view_index, 4);
  constexpr int kStringIds[] = {IDS_ASH_DESKS_DESK_1_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_2_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_3_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_4_MINI_VIEW_TITLE};

  return l10n_util::GetStringUTF16(kStringIds[mini_view_index]);
}

}  // namespace

DesksBarView::DesksBarView()
    : backgroud_view_(new views::View),
      new_desk_button_(new NewDeskButton(this)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  backgroud_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  backgroud_view_->layer()->SetFillsBoundsOpaquely(false);
  backgroud_view_->layer()->SetColor(SkColorSetARGB(60, 0, 0, 0));

  AddChildView(backgroud_view_);
  AddChildView(new_desk_button_);
  UpdateNewDeskButtonState();
  DesksController::Get()->AddObserver(this);
}

DesksBarView::~DesksBarView() {
  DesksController::Get()->RemoveObserver(this);
}

// static
int DesksBarView::GetBarHeight() {
  return kBarHeight;
}

void DesksBarView::Init() {
  UpdateNewMiniViews();
}

const char* DesksBarView::GetClassName() const {
  return "DesksBarView";
}

void DesksBarView::Layout() {
  backgroud_view_->SetBoundsRect(bounds());

  constexpr int kButtonRightMargin = 36;
  constexpr int kIconAndTextHorizontalPadding = 16;
  constexpr int kIconAndTextVerticalPadding = 8;

  gfx::Size new_desk_button_size = new_desk_button_->GetPreferredSize();
  new_desk_button_size.Enlarge(2 * kIconAndTextHorizontalPadding,
                               2 * kIconAndTextVerticalPadding);

  const gfx::Rect button_bounds{
      bounds().right() - new_desk_button_size.width() - kButtonRightMargin,
      (bounds().height() - new_desk_button_size.height()) / 2,
      new_desk_button_size.width(), new_desk_button_size.height()};
  new_desk_button_->SetBoundsRect(button_bounds);

  if (mini_views_.empty())
    return;

  constexpr int kMiniViewsSpacing = 8;
  const gfx::Size mini_view_size = mini_views_[0]->GetPreferredSize();
  const int total_width =
      mini_views_.size() * (mini_view_size.width() + kMiniViewsSpacing) -
      kMiniViewsSpacing;
  gfx::Rect mini_views_bounds = bounds();
  mini_views_bounds.ClampToCenteredSize(
      gfx::Size(total_width, mini_view_size.height()));

  int x = mini_views_bounds.x();
  const int y = mini_views_bounds.y();
  for (auto& mini_view : mini_views_) {
    mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
    x += (mini_view_size.width() + kMiniViewsSpacing);
  }
}

void DesksBarView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  auto* controller = DesksController::Get();
  if (sender == new_desk_button_ && controller->CanCreateDesks()) {
    controller->NewDesk();
  } else {
    // TODO(afakhry): Handle mini_view presses.
  }

  UpdateNewDeskButtonState();
}

void DesksBarView::OnDeskAdded(const Desk* desk) {
  UpdateNewMiniViews();
}

void DesksBarView::OnDeskRemoved(const Desk* desk) {
  auto iter =
      std::find_if(mini_views_.begin(), mini_views_.end(),
                   [desk](const std::unique_ptr<DeskMiniView>& mini_view) {
                     return desk == mini_view->desk();
                   });

  DCHECK(iter != mini_views_.end());

  mini_views_.erase(iter);
  Layout();
  UpdateMiniViewsLabels();

  // TODO(afakhry): Add animations.

  UpdateNewDeskButtonState();
}

void DesksBarView::UpdateNewDeskButtonState() {
  new_desk_button_->SetEnabled(DesksController::Get()->CanCreateDesks());
}

void DesksBarView::UpdateNewMiniViews() {
  const auto& desks = DesksController::Get()->desks();
  if (desks.size() < 2) {
    // We do not show mini_views when we have a single desk.
    DCHECK(mini_views_.empty());
    return;
  }

  // This should not be called when a desk is removed.
  DCHECK_LE(mini_views_.size(), desks.size());

  for (const auto& desk : desks) {
    if (!FindMiniViewForDesk(desk.get())) {
      mini_views_.emplace_back(std::make_unique<DeskMiniView>(
          desk.get(), GetMiniViewTitle(mini_views_.size()), this));
      DeskMiniView* mini_view = mini_views_.back().get();
      mini_view->set_owned_by_client();
      AddChildView(mini_view);
    }
  }

  Layout();

  // TODO(afakhry): Add animations.
}

DeskMiniView* DesksBarView::FindMiniViewForDesk(const Desk* desk) const {
  for (auto& mini_view : mini_views_) {
    if (mini_view->desk() == desk)
      return mini_view.get();
  }

  return nullptr;
}

void DesksBarView::UpdateMiniViewsLabels() {
  // TODO(afakhry): Don't do this for user-modified desk labels.
  size_t i = 0;
  for (auto& mini_view : mini_views_)
    mini_view->SetTitle(GetMiniViewTitle(i++));
}

}  // namespace ash
