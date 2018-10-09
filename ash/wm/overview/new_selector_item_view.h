// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_NEW_SELECTOR_ITEM_VIEW_H_
#define ASH_WM_OVERVIEW_NEW_SELECTOR_ITEM_VIEW_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

// TODO(minch): Rename new selector item to drop target.
// NewSelectorItemView represents a transparent view with border in overview. It
// includes a background view and plus icon. Dragged window in tablet mode can
// be dragged into it and then dropped into overview.
class NewSelectorItemView : public views::View {
 public:
  // The amount of rounding on window edges in overview mode.
  static constexpr int kOverviewWindowRoundingDp = 4;

  explicit NewSelectorItemView(bool has_plus_icon);
  ~NewSelectorItemView() override = default;

  // Updates the visibility of |background_view_| since it is only shown when
  // new selector item is selected in overview.
  void UpdateBackgroundVisibility(bool visible);

  // views::View:
  void Layout() override;

 private:
  class PlusIconView;

  views::View* background_view_ = nullptr;
  PlusIconView* plus_icon_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NewSelectorItemView);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_NEW_SELECTOR_ITEM_VIEW_H_
