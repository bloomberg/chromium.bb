// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_experimental.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/view_ids.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace {

// Returns the width of the tab endcap in DIP.  More precisely, this is the
// width of the curve making up either the outer or inner edge of the stroke;
// since these two curves are horizontally offset by 1 px (regardless of scale),
// the total width of the endcap from tab outer edge to the inside end of the
// stroke inner edge is (GetUnscaledEndcapWidth() * scale) + 1.
//
// The value returned here must be at least Tab::kMinimumEndcapWidth.
float GetTabEndcapWidth() {
  return GetLayoutInsets(TAB).left() - 0.5f;
}

}

TabExperimental::TabExperimental()
    : views::View(),
      title_(new views::Label),
      hover_controller_(this),
      paint_(this) {
  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::FADE_TAIL);
  title_->SetHandlesTooltips(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetText(CoreTabHelper::GetDefaultTitle());
  AddChildView(title_);

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  set_notify_enter_exit_on_child(true);

  set_id(VIEW_ID_TAB);

  // This will cause calls to GetContentsBounds to return only the rectangle
  // inside the tab shape, rather than to its extents.
  SetBorder(views::CreateEmptyBorder(GetLayoutInsets(TAB)));
}

TabExperimental::~TabExperimental() {}

void TabExperimental::SetActive(bool active) {
  if (active != active_) {
    active_ = active;
    SchedulePaint();
  }
}

void TabExperimental::SetSelected(bool selected) {
  if (selected != selected_) {
    selected_ = selected;
    SchedulePaint();
  }
}

void TabExperimental::SetTitle(const base::string16& title) {
  title_->SetText(title);
}

int TabExperimental::GetOverlap() {
  // We want to overlap the endcap portions entirely.
  return gfx::ToCeiledInt(GetTabEndcapWidth());
}

void TabExperimental::OnPaint(gfx::Canvas* canvas) {
  paint_.PaintTabBackground(canvas, active_, 0, 0, nullptr);
}

void TabExperimental::Layout() {
  constexpr int kTitleSpacing = 6;
  const gfx::Rect bounds = GetContentsBounds();

  title_->SetBoundsRect(gfx::Rect(bounds.x() + kTitleSpacing, bounds.y(),
                                  bounds.width() - (kTitleSpacing * 2),
                                  bounds.height()));
}
