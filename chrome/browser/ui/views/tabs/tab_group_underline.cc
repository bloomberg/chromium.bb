// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_underline.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

constexpr int TabGroupUnderline::kStrokeThickness;

TabGroupUnderline::TabGroupUnderline(TabStrip* tab_strip, TabGroupId group)
    : tab_strip_(tab_strip), group_(group) {
  UpdateBounds();
}

void TabGroupUnderline::OnPaint(gfx::Canvas* canvas) {
  UpdateBounds();

  SkPath path = GetPath();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetColor());
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(path, flags);
}

void TabGroupUnderline::UpdateBounds() {
  const int start_x = GetStart();
  const int end_x = GetEnd();

  const int start_y = tab_strip_->bounds().height() - 1;

  SetBounds(start_x, start_y - kStrokeThickness, end_x - start_x,
            kStrokeThickness);
}

int TabGroupUnderline::GetStart() const {
  const TabGroupHeader* group_header = tab_strip_->group_header(group_);

  constexpr int kInset = 20;
  return group_header->bounds().x() + kInset;
}

int TabGroupUnderline::GetEnd() const {
  // Fall back to the group header end for any corner cases. This ensures
  // that the underline always has a positive width.
  const TabGroupHeader* group_header = tab_strip_->group_header(group_);
  constexpr int kInset = 20;
  const int header_end = group_header->bounds().right() - kInset;

  const std::vector<int> tabs_in_group =
      tab_strip_->controller()->ListTabsInGroup(group_);
  if (tabs_in_group.size() <= 0)
    return header_end;

  const int last_tab_index = tabs_in_group[tabs_in_group.size() - 1];
  const Tab* last_tab = tab_strip_->tab_at(last_tab_index);

  const int tab_end = last_tab->bounds().right() +
                      (last_tab->IsActive() ? kStrokeThickness : -kInset);
  return std::max(tab_end, header_end);
}

SkPath TabGroupUnderline::GetPath() const {
  SkPath path;

  path.moveTo(0, kStrokeThickness);
  path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPath::kCW_Direction, kStrokeThickness, 0);
  path.lineTo(width() - kStrokeThickness, 0);
  path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPath::kCW_Direction, width(), kStrokeThickness);
  path.close();

  return path;
}

SkColor TabGroupUnderline::GetColor() const {
  const TabGroupVisualData* data =
      tab_strip_->controller()->GetVisualDataForGroup(group_);

  return data->color();
}
