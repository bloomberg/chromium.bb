// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"

#include <memory>

#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kDotCount = 3;
constexpr int kDotSizeDip = 6;
constexpr int kSpacingDip = 4;

// DotView ---------------------------------------------------------------------

class DotView : public views::View {
 public:
  DotView() = default;
  ~DotView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kDotSizeDip, GetHeightForWidth(kDotSizeDip));
  }

  int GetHeightForWidth(int width) const override { return kDotSizeDip; }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleGrey300);

    const gfx::Rect& b = GetContentsBounds();
    const gfx::Point center = gfx::Point(b.width() / 2, b.height() / 2);

    canvas->DrawCircle(center, kDotSizeDip / 2, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DotView);
};

}  // namespace

// AssistantProgressIndicator --------------------------------------------------

AssistantProgressIndicator::AssistantProgressIndicator() {
  InitLayout();
}

AssistantProgressIndicator::~AssistantProgressIndicator() = default;

void AssistantProgressIndicator::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), kSpacingDip));

  // Dots.
  for (int i = 0; i < kDotCount; ++i)
    AddChildView(new DotView());
}

}  // namespace ash