// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/hosted_app_origin_text.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace {

constexpr base::TimeDelta kOriginSlideInDuration =
    base::TimeDelta::FromMilliseconds(800);
constexpr base::TimeDelta kOriginPauseDuration =
    base::TimeDelta::FromMilliseconds(2500);
constexpr base::TimeDelta kOriginSlideOutDuration =
    base::TimeDelta::FromMilliseconds(800);

constexpr gfx::Tween::Type kTweenType = gfx::Tween::FAST_OUT_SLOW_IN_2;

}  // namespace

HostedAppOriginText::HostedAppOriginText(Browser* browser) {
  DCHECK(
      extensions::HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
          browser));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  label_ = std::make_unique<views::Label>(
               browser->hosted_app_controller()->GetFormattedUrlOrigin(),
               ChromeTextContext::CONTEXT_BODY_TEXT_SMALL,
               ChromeTextStyle::STYLE_EMPHASIZED)
               .release();
  label_->SetElideBehavior(gfx::ELIDE_HEAD);
  label_->SetSubpixelRenderingEnabled(false);
  // Disable Label's auto readability to ensure consistent colors in the title
  // bar (see http://crbug.com/814121#c2).
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->layer()->SetOpacity(0);
  AddChildView(label_);

  // Clip child views to this view.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
}

HostedAppOriginText::~HostedAppOriginText() = default;

void HostedAppOriginText::SetTextColor(SkColor color) {
  label_->SetEnabledColor(color);
}

void HostedAppOriginText::StartSlideAnimation() {
  ui::Layer* label_layer = label_->layer();

  // Current state will become the first animation keyframe.
  DCHECK_EQ(label_layer->opacity(), 0);

  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();

  // Slide in.
  auto opacity_keyframe = ui::LayerAnimationElement::CreateOpacityElement(
      1, kOriginSlideInDuration);
  opacity_keyframe->set_tween_type(kTweenType);
  opacity_sequence->AddElement(std::move(opacity_keyframe));

  // Pause.
  opacity_sequence->AddElement(
      ui::LayerAnimationElement::CreatePauseElement(0, kOriginPauseDuration));

  // Slide out.
  opacity_keyframe = ui::LayerAnimationElement::CreateOpacityElement(
      0, kOriginSlideOutDuration);
  opacity_keyframe->set_tween_type(kTweenType);
  opacity_sequence->AddElement(std::move(opacity_keyframe));

  label_layer->GetAnimator()->StartAnimation(opacity_sequence.release());

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HostedAppOriginText::AnimationComplete,
                     weak_factory_.GetWeakPtr()),
      AnimationDuration());

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void HostedAppOriginText::AnimationComplete() {
  SetVisible(false);
}

base::TimeDelta HostedAppOriginText::AnimationDuration() {
  return kOriginSlideInDuration + kOriginPauseDuration +
         kOriginSlideOutDuration;
}

void HostedAppOriginText::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kApplication;
  node_data->SetName(label_->text());
}
