// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/overview_toolbar.h"

#include "athena/resources/grit/athena_resources.h"
#include "athena/strings/grit/athena_strings.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform.h"

namespace {

const int kActionButtonImageSize = 54;
const int kActionButtonTextSize = 20;
const int kActionButtonPaddingFromRight = 32;
}

namespace athena {

class ActionButton : public ui::LayerDelegate {
 public:
  ActionButton(int resource_id, const base::string16& label)
      : resource_id_(resource_id), label_(label) {
    layer_.reset(new ui::Layer(ui::LAYER_TEXTURED));
    layer_->set_delegate(this);
    layer_->SetFillsBoundsOpaquely(false);
    layer_->SetVisible(true);
    layer_->SetOpacity(0);
  }

  virtual ~ActionButton() {}

  static void DestroyAfterFadeout(scoped_ptr<ActionButton> button) {
    ui::Layer* layer = button->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&ActionButton::DestroyImmediately, base::Passed(&button))));
    layer->SetOpacity(0);
  }

  void SetPosition(const gfx::Point& position) {
    layer_->SetBounds(
        gfx::Rect(position,
                  gfx::Size(kActionButtonImageSize,
                            kActionButtonImageSize + kActionButtonTextSize)));
  }

  ui::Layer* layer() { return layer_.get(); }

 private:
  static void DestroyImmediately(scoped_ptr<ActionButton> button) {
    button.reset();
  }

  // ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    canvas->DrawImageInt(*bundle.GetImageSkiaNamed(resource_id_), 0, 0);
    gfx::ShadowValues shadow;
    shadow.push_back(gfx::ShadowValue(gfx::Point(0, 1), 2, SK_ColorBLACK));
    shadow.push_back(gfx::ShadowValue(gfx::Point(0, -1), 2, SK_ColorBLACK));
    canvas->DrawStringRectWithShadows(label_,
                                      gfx::FontList(),
                                      SK_ColorWHITE,
                                      gfx::Rect(0,
                                                kActionButtonImageSize,
                                                kActionButtonImageSize,
                                                kActionButtonTextSize),
                                      0,
                                      gfx::Canvas::TEXT_ALIGN_CENTER,
                                      shadow);
  }

  virtual void OnDelegatedFrameDamage(
      const gfx::Rect& damage_rect_in_dip) OVERRIDE {}

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  int resource_id_;
  base::string16 label_;
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(ActionButton);
};

OverviewToolbar::OverviewToolbar(aura::Window* container)
    : shown_(false),
      disabled_action_bitfields_(0),
      close_(new ActionButton(
          IDR_ATHENA_OVERVIEW_TRASH,
          l10n_util::GetStringUTF16(IDS_ATHENA_OVERVIEW_CLOSE))),
      split_(new ActionButton(
          IDR_ATHENA_OVERVIEW_SPLIT,
          l10n_util::GetStringUTF16(IDS_ATHENA_OVERVIEW_SPLIT))),
      current_action_(ACTION_TYPE_NONE),
      container_bounds_(container->bounds()) {
  const int kPaddingFromBottom = 200;
  const int kPaddingBetweenButtons = 200;

  int x = container_bounds_.right() -
          (kActionButtonPaddingFromRight + kActionButtonImageSize);
  int y = container_bounds_.bottom() -
          (kPaddingFromBottom + kActionButtonImageSize);
  split_->SetPosition(gfx::Point(x, y));
  y -= kPaddingBetweenButtons;
  close_->SetPosition(gfx::Point(x, y));

  container->layer()->Add(split_->layer());
  container->layer()->Add(close_->layer());
}

OverviewToolbar::~OverviewToolbar() {
  // If the buttons are visible, then fade them out, instead of destroying them
  // immediately.
  if (shown_) {
    ActionButton::DestroyAfterFadeout(split_.Pass());
    ActionButton::DestroyAfterFadeout(close_.Pass());
  }
}

OverviewToolbar::ActionType OverviewToolbar::GetHighlightAction(
    const ui::GestureEvent& event) const {
  if (IsActionEnabled(ACTION_TYPE_SPLIT) &&
      IsEventOverButton(split_.get(), event))
    return ACTION_TYPE_SPLIT;
  if (IsActionEnabled(ACTION_TYPE_CLOSE) &&
      IsEventOverButton(close_.get(), event))
    return ACTION_TYPE_CLOSE;
  return ACTION_TYPE_NONE;
}

void OverviewToolbar::SetHighlightAction(ActionType action) {
  CHECK(IsActionEnabled(action));
  if (current_action_ == action)
    return;
  current_action_ = action;
  if (!shown_) {
    ShowActionButtons();
  } else {
    TransformButton(close_.get());
    TransformButton(split_.get());
  }
}

void OverviewToolbar::ShowActionButtons() {
  if (!shown_)
    ToggleActionButtonsVisibility();
}

void OverviewToolbar::HideActionButtons() {
  if (shown_)
    ToggleActionButtonsVisibility();
}

void OverviewToolbar::DisableAction(ActionType action) {
  CHECK_NE(current_action_, action);
  disabled_action_bitfields_ |= (1u << action);
}

void OverviewToolbar::ToggleActionButtonsVisibility() {
  shown_ = !shown_;
  TransformButton(close_.get());
  TransformButton(split_.get());
}

bool OverviewToolbar::IsActionEnabled(ActionType action) const {
  return !(disabled_action_bitfields_ & (1u << action));
}

bool OverviewToolbar::IsEventOverButton(ActionButton* button,
                                        const ui::GestureEvent& event) const {
  const int kBoundsInsetForTarget = 30;
  gfx::RectF bounds = button->layer()->bounds();
  bounds.Inset(-kBoundsInsetForTarget, -kBoundsInsetForTarget);
  return bounds.Contains(event.location());
}

gfx::Transform OverviewToolbar::ComputeTransformFor(
    ActionButton* button) const {
  if (!shown_)
    return gfx::Transform();

  const float kHighlightScale = 1.5;
  bool button_is_highlighted =
      (current_action_ == ACTION_TYPE_CLOSE && button == close_.get()) ||
      (current_action_ == ACTION_TYPE_SPLIT && button == split_.get());
  gfx::Transform transform;
  if (button_is_highlighted) {
    transform.Translate(-kActionButtonImageSize * (kHighlightScale - 1) / 2, 0);
    transform.Scale(kHighlightScale, kHighlightScale);
  }
  return transform;
}

void OverviewToolbar::TransformButton(ActionButton* button) {
  ui::ScopedLayerAnimationSettings split_settings(
      button->layer()->GetAnimator());
  split_settings.SetTweenType(gfx::Tween::SMOOTH_IN_OUT);
  button->layer()->SetTransform(ComputeTransformFor(button));
  bool button_is_enabled =
      (button == close_.get() && IsActionEnabled(ACTION_TYPE_CLOSE)) ||
      (button == split_.get() && IsActionEnabled(ACTION_TYPE_SPLIT));
  button->layer()->SetOpacity((button_is_enabled && shown_) ? 1 : 0);
}

}  // namespace athena
