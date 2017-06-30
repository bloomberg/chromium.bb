// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_view.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Spacing applied to all sides of the border of the control.
constexpr int kSpacingInsetsAllSides = 10;

// Spacing applied between controls of FullscreenControlView.
constexpr int kBetweenChildSpacing = 10;

}  // namespace

FullscreenControlView::FullscreenControlView(BrowserView* browser_view)
    : browser_view_(browser_view),
      exit_fullscreen_button_(new views::LabelButton(
          this,
          l10n_util::GetStringUTF16(
              IDS_FULLSCREEN_EXIT_CONTROL_EXIT_FULLSCREEN))) {
  AddChildView(exit_fullscreen_button_);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        gfx::Insets(kSpacingInsetsAllSides),
                                        kBetweenChildSpacing));
  // TODO(robliao): If we decide to move forward with this, use themes.
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
}

FullscreenControlView::~FullscreenControlView() = default;

void FullscreenControlView::SetFocusAndSelection(bool select_all) {}

void FullscreenControlView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == exit_fullscreen_button_)
    browser_view_->ExitFullscreen();
}
