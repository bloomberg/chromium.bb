// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"

#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/compositor/layer.h"
#include "ui/views/painter.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/shadow_controller.h"
#endif

namespace {

// Value from the spec controlling appearance of the shadow.
constexpr int kElevation = 16;

// Insets used to position |contents_| within |contents_host_|.
gfx::Insets GetContentInsets(views::View* location_bar) {
  return gfx::Insets(
             RoundedOmniboxResultsFrame::kLocationBarAlignmentInsets.top(), 0,
             0, 0) +
         gfx::Insets(location_bar->height(), 0, 0, 0);
}

#if defined(USE_AURA)
// A ui::EventTargeter that allows mouse and touch events in the top portion of
// the Widget to pass through to the omnibox beneath it.
class ResultsTargeter : public aura::WindowTargeter {
 public:
  explicit ResultsTargeter(int top_inset) {
    const gfx::Insets event_insets(top_inset, 0, 0, 0);
    SetInsets(event_insets, event_insets);
  }
};
#endif  // USE_AURA

}  // namespace

constexpr gfx::Insets RoundedOmniboxResultsFrame::kLocationBarAlignmentInsets;

RoundedOmniboxResultsFrame::RoundedOmniboxResultsFrame(
    views::View* contents,
    LocationBarView* location_bar)
    : content_insets_(GetContentInsets(location_bar)), contents_(contents) {
  // Host the contents in its own View to simplify layout and clipping.
  contents_host_ = new views::View();
  contents_host_->SetPaintToLayer();
  contents_host_->layer()->SetFillsBoundsOpaquely(false);

  // Use a solid background. Note this is clipped to get rounded corners.
  SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ResultsTableNormalBackground);
  contents_host_->SetBackground(views::CreateSolidBackground(background_color));

  // Use a textured mask to clip contents. This doesn't work on Windows
  // (https://crbug.com/713359), and can't be animated, but it simplifies
  // selection highlights.
  // TODO(tapted): Remove this and have the contents paint a half-rounded rect
  // for the background, and when selecting the bottom row.
  contents_mask_ = views::Painter::CreatePaintedLayer(
      views::Painter::CreateSolidRoundRectPainter(
          SK_ColorBLACK, GetLayoutConstant(LOCATION_BAR_BUBBLE_CORNER_RADIUS)));
  contents_mask_->layer()->SetFillsBoundsOpaquely(false);
  contents_host_->layer()->SetMaskLayer(contents_mask_->layer());

  // Paint the omnibox border with transparent pixels to make a hole.
  views::View* top_background = new views::View();
  auto background = std::make_unique<BackgroundWith1PxBorder>(
      SK_ColorTRANSPARENT, background_color);
  background->set_blend_mode(SkBlendMode::kSrc);
  top_background->SetBackground(std::move(background));
  gfx::Size location_bar_size = location_bar->bounds().size();
  top_background->SetBounds(
      kLocationBarAlignmentInsets.left(), kLocationBarAlignmentInsets.top(),
      location_bar_size.width(), location_bar_size.height());

  contents_host_->AddChildView(top_background);
  contents_host_->AddChildView(contents_);
  AddChildView(contents_host_);
}

RoundedOmniboxResultsFrame::~RoundedOmniboxResultsFrame() = default;

// static
void RoundedOmniboxResultsFrame::OnBeforeWidgetInit(
    views::Widget::InitParams* params) {
  params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_DROP;
  params->corner_radius = GetLayoutConstant(LOCATION_BAR_BUBBLE_CORNER_RADIUS);
  params->shadow_elevation = kElevation;
  params->name = "RoundedOmniboxResultsFrameWindow";
}

// static
gfx::Insets RoundedOmniboxResultsFrame::GetAlignmentInsets(
    views::View* location_bar) {
  // Note this insets the location bar height from the bottom to align correctly
  // (not the top).
  return kLocationBarAlignmentInsets +
         gfx::Insets(0, 0, location_bar->height(), 0);
}

const char* RoundedOmniboxResultsFrame::GetClassName() const {
  return "RoundedOmniboxResultsFrame";
}

void RoundedOmniboxResultsFrame::Layout() {
  // This is called when the Widget resizes due to results changing. Resizing
  // the Widget is fast on ChromeOS, but slow on other platforms, and can't be
  // animated smoothly.
  // TODO(tapted): Investigate using a static Widget size.
  gfx::Rect bounds = GetLocalBounds();
  contents_host_->SetBoundsRect(bounds);
  contents_mask_->layer()->SetBounds(gfx::Rect(bounds.size()));

  gfx::Rect results_bounds = gfx::Rect(bounds.size());
  results_bounds.Inset(content_insets_);
  contents_->SetBoundsRect(results_bounds);
}

void RoundedOmniboxResultsFrame::AddedToWidget() {
#if defined(USE_AURA)
  GetWidget()->GetNativeWindow()->SetEventTargeter(
      std::make_unique<ResultsTargeter>(content_insets_.top()));
#endif
}
