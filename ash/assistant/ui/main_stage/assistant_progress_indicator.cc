// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"

#include <algorithm>

#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kDotCount = 3;
constexpr float kDotLargeSizeDip = 9.f;
constexpr float kDotSmallSizeDip = 6.f;
constexpr int kEmbeddedUiPreferredHeightDip = 9;
constexpr int kSpacingDip = 4;

// Animation.
constexpr base::TimeDelta kAnimationOffsetDuration =
    base::TimeDelta::FromMilliseconds(216);
constexpr base::TimeDelta kAnimationPauseDuration =
    base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kAnimationScaleUpDuration =
    base::TimeDelta::FromMilliseconds(266);
constexpr base::TimeDelta kAnimationScaleDownDuration =
    base::TimeDelta::FromMilliseconds(450);

// Transformation.
constexpr float kScaleFactor = kDotLargeSizeDip / kDotSmallSizeDip;
constexpr float kTranslationDip = -(kDotLargeSizeDip - kDotSmallSizeDip) / 2.f;

// DotBackground ---------------------------------------------------------------

class DotBackground : public views::Background {
 public:
  DotBackground() = default;
  ~DotBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleGrey300);

    const gfx::Rect& b = view->GetContentsBounds();
    const gfx::Point center = gfx::Point(b.width() / 2, b.height() / 2);
    const int radius = std::min(b.width() / 2, b.height() / 2);

    canvas->DrawCircle(center, radius, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DotBackground);
};

}  // namespace

// AssistantProgressIndicator --------------------------------------------------

AssistantProgressIndicator::AssistantProgressIndicator() {
  InitLayout();
}

AssistantProgressIndicator::~AssistantProgressIndicator() = default;

const char* AssistantProgressIndicator::GetClassName() const {
  return "AssistantProgressIndicator";
}

gfx::Size AssistantProgressIndicator::CalculatePreferredSize() const {
  const int preferred_width = views::View::CalculatePreferredSize().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int AssistantProgressIndicator::GetHeightForWidth(int width) const {
  return app_list_features::IsEmbeddedAssistantUIEnabled()
             ? kEmbeddedUiPreferredHeightDip
             : views::View::GetHeightForWidth(width);
}

void AssistantProgressIndicator::AddedToWidget() {
  VisibilityChanged(/*starting_from=*/this, /*is_visible=*/visible());
}

void AssistantProgressIndicator::RemovedFromWidget() {
  VisibilityChanged(/*starting_from=*/this, /*is_visible=*/false);
}

void AssistantProgressIndicator::OnLayerOpacityChanged(
    ui::PropertyChangeReason reason) {
  VisibilityChanged(/*starting_from=*/this,
                    /*is_visible=*/visible());
}

void AssistantProgressIndicator::VisibilityChanged(views::View* starting_from,
                                                   bool is_visible) {
  // Stop the animation when the view is either not visible or is "visible" but
  // not actually visible to the user (because it is faded out).
  const bool is_drawn =
      IsDrawn() && !cc::MathUtil::IsWithinEpsilon(layer()->opacity(), 0.f);
  if (is_drawn_ == is_drawn)
    return;

  is_drawn_ = is_drawn;

  if (!is_drawn_) {
    // Stop all animations.
    for (int i = 0; i < child_count(); ++i) {
      child_at(i)->layer()->GetAnimator()->StopAnimating();
    }
    return;
  }

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateTransformElement;

  // The animation performs scaling on the child views. In order to give the
  // illusion that scaling is being performed about the center of the view as
  // the transformation origin, we also need to perform a translation.
  gfx::Transform transform;
  transform.Translate(kTranslationDip, kTranslationDip);
  transform.Scale(kScaleFactor, kScaleFactor);

  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);

    if (i > 0) {
      // Schedule the animations to start after an offset.
      view->layer()->GetAnimator()->SchedulePauseForProperties(
          i * kAnimationOffsetDuration,
          ui::LayerAnimationElement::AnimatableProperty::TRANSFORM);
    }

    // Schedule transformation animation.
    view->layer()->GetAnimator()->ScheduleAnimation(
        CreateLayerAnimationSequence(
            // Animate scale up.
            CreateTransformElement(transform, kAnimationScaleUpDuration),
            // Animate scale down.
            CreateTransformElement(gfx::Transform(),
                                   kAnimationScaleDownDuration),
            // Pause before next iteration.
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                kAnimationPauseDuration),
            // Animation parameters.
            {.is_cyclic = true}));
  }
}

void AssistantProgressIndicator::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Initialize dots.
  for (int i = 0; i < kDotCount; ++i) {
    views::View* dot_view = new views::View();
    dot_view->SetBackground(std::make_unique<DotBackground>());
    dot_view->SetPreferredSize(gfx::Size(kDotSmallSizeDip, kDotSmallSizeDip));

    // Dots will animate on their own layers.
    dot_view->SetPaintToLayer();
    dot_view->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(dot_view);
  }
}

}  // namespace ash
