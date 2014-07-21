// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/transparent_activate_window_button.h"
#include "base/auto_reset.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::Widget* CreateCloseWindowButton(aura::Window* root_window,
                                       views::ButtonListener* listener) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, ash::kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(params);
  views::ImageButton* button = new views::ImageButton(listener);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  button->SetImage(views::CustomButton::STATE_NORMAL,
                   rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE));
  button->SetImage(views::CustomButton::STATE_HOVERED,
                   rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_H));
  button->SetImage(views::CustomButton::STATE_PRESSED,
                   rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE_P));
  widget->SetContentsView(button);
  widget->SetSize(rb.GetImageSkiaNamed(IDR_AURA_WINDOW_OVERVIEW_CLOSE)->size());
  widget->Show();
  return widget;
}

}  // namespace

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
static const int kWindowMargin = 30;

// Foreground label color.
static const SkColor kLabelColor = SK_ColorWHITE;

// Background label color.
static const SkColor kLabelBackground = SK_ColorTRANSPARENT;

// Label shadow color.
static const SkColor kLabelShadow = 0xB0000000;

// Vertical padding for the label, both over and beneath it.
static const int kVerticalLabelPadding = 20;

// Solid shadow length from the label
static const int kVerticalShadowOffset = 1;

// Amount of blur applied to the label shadow
static const int kShadowBlur = 10;

const int WindowSelectorItem::kFadeInMilliseconds = 80;

// Opacity for dimmed items.
static const float kDimmedItemOpacity = 0.5f;

WindowSelectorItem::WindowSelectorItem()
    : dimmed_(false),
      root_window_(NULL),
      in_bounds_update_(false),
      window_label_view_(NULL) {
}

WindowSelectorItem::~WindowSelectorItem() {
}

void WindowSelectorItem::RemoveWindow(const aura::Window* window) {
  // If empty WindowSelectorItem will be destroyed immediately after this by
  // its owner.
  if (empty())
    return;
  window_label_.reset();
  UpdateWindowLabels(target_bounds_, root_window_, false);
  UpdateCloseButtonBounds(root_window_, false);
}

void WindowSelectorItem::SetBounds(aura::Window* root_window,
                                   const gfx::Rect& target_bounds,
                                   bool animate) {
  if (in_bounds_update_)
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  root_window_ = root_window;
  target_bounds_ = target_bounds;

  // Set the bounds of the transparent window handler to cover the entire
  // bounding box area.
  if (!activate_window_button_) {
    activate_window_button_.reset(
        new TransparentActivateWindowButton(SelectionWindow()));
  }
  activate_window_button_->SetBounds(target_bounds);

  UpdateWindowLabels(target_bounds, root_window, animate);

  gfx::Rect inset_bounds(target_bounds);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(root_window, inset_bounds, animate);
  UpdateCloseButtonBounds(root_window, animate);
}

void WindowSelectorItem::RecomputeWindowTransforms() {
  if (in_bounds_update_ || target_bounds_.IsEmpty())
    return;
  DCHECK(root_window_);
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  gfx::Rect inset_bounds(target_bounds_);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(root_window_, inset_bounds, false);
  UpdateCloseButtonBounds(root_window_, false);
}

void WindowSelectorItem::SendFocusAlert() const {
  activate_window_button_->SendFocusAlert();
}

void WindowSelectorItem::SetDimmed(bool dimmed) {
  dimmed_ = dimmed;
  SetOpacity(dimmed ? kDimmedItemOpacity : 1.0f);
}

void WindowSelectorItem::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  views::Widget::GetWidgetForNativeView(SelectionWindow())->Close();
}

void WindowSelectorItem::OnWindowTitleChanged(aura::Window* window) {
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  if (window == SelectionWindow())
    window_label_view_->SetText(window->title());
}

void WindowSelectorItem::UpdateCloseButtonBounds(aura::Window* root_window,
                                                 bool animate) {
  gfx::RectF align_bounds(SelectionWindow()->layer()->bounds());
  gfx::Transform window_transform;
  window_transform.Translate(align_bounds.x(), align_bounds.y());
  window_transform.PreconcatTransform(SelectionWindow()->layer()->
                                          GetTargetTransform());
  window_transform.Translate(-align_bounds.x(), -align_bounds.y());
  window_transform.TransformRect(&align_bounds);
  gfx::Rect target_bounds = ToEnclosingRect(align_bounds);

  gfx::Transform close_button_transform;
  close_button_transform.Translate(target_bounds.right(), target_bounds.y());

  // If the root window has changed, force the close button to be recreated
  // and faded in on the new root window.
  if (close_button_ &&
      close_button_->GetNativeWindow()->GetRootWindow() != root_window) {
    close_button_.reset();
  }

  if (!close_button_) {
    close_button_.reset(CreateCloseWindowButton(root_window, this));
    gfx::Rect close_button_rect(close_button_->GetNativeWindow()->bounds());
    // Align the center of the button with position (0, 0) so that the
    // translate transform does not need to take the button dimensions into
    // account.
    close_button_rect.set_x(-close_button_rect.width() / 2);
    close_button_rect.set_y(-close_button_rect.height() / 2);
    close_button_->GetNativeWindow()->SetBounds(close_button_rect);
    close_button_->GetNativeWindow()->SetTransform(close_button_transform);
    // The close button is initialized when entering overview, fade the button
    // in after the window should be in place.
    ui::Layer* layer = close_button_->GetNativeWindow()->layer();
    layer->SetOpacity(0);
    layer->GetAnimator()->StopAnimating();
    layer->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(
            ScopedTransformOverviewWindow::kTransitionMilliseconds),
        ui::LayerAnimationElement::OPACITY);
    {
      ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          WindowSelectorItem::kFadeInMilliseconds));
      layer->SetOpacity(1);
    }
  } else {
    if (animate) {
      ui::ScopedLayerAnimationSettings settings(
          close_button_->GetNativeWindow()->layer()->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          ScopedTransformOverviewWindow::kTransitionMilliseconds));
      close_button_->GetNativeWindow()->SetTransform(close_button_transform);
    } else {
      close_button_->GetNativeWindow()->SetTransform(close_button_transform);
    }
  }
}

void WindowSelectorItem::SetOpacity(float opacity) {
  window_label_->GetNativeWindow()->layer()->SetOpacity(opacity);
  close_button_->GetNativeWindow()->layer()->SetOpacity(opacity);
}

void WindowSelectorItem::UpdateWindowLabels(const gfx::Rect& window_bounds,
                                            aura::Window* root_window,
                                            bool animate) {
  gfx::Rect converted_bounds = ScreenUtil::ConvertRectFromScreen(root_window,
                                                                 window_bounds);
  gfx::Rect label_bounds(converted_bounds.x(),
                         converted_bounds.bottom(),
                         converted_bounds.width(),
                         0);

  // If the root window has changed, force the window label to be recreated
  // and faded in on the new root window.
  if (window_label_ &&
      window_label_->GetNativeWindow()->GetRootWindow() != root_window) {
    window_label_.reset();
  }

  if (!window_label_) {
    CreateWindowLabel(SelectionWindow()->title());
    label_bounds.set_height(window_label_view_->GetPreferredSize().height());
    label_bounds.set_y(label_bounds.y() - window_label_view_->
                           GetPreferredSize().height());
    window_label_->GetNativeWindow()->SetBounds(label_bounds);
    ui::Layer* layer = window_label_->GetNativeWindow()->layer();

    layer->SetOpacity(0);
    layer->GetAnimator()->StopAnimating();

    layer->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(
            ScopedTransformOverviewWindow::kTransitionMilliseconds),
        ui::LayerAnimationElement::OPACITY);

    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kFadeInMilliseconds));
    layer->SetOpacity(1);
  } else {
    label_bounds.set_height(window_label_->
                                GetContentsView()->GetPreferredSize().height());
    label_bounds.set_y(label_bounds.y() - window_label_->
                           GetContentsView()->GetPreferredSize().height());
    if (animate) {
      ui::ScopedLayerAnimationSettings settings(
          window_label_->GetNativeWindow()->layer()->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          ScopedTransformOverviewWindow::kTransitionMilliseconds));
      window_label_->GetNativeWindow()->SetBounds(label_bounds);
    } else {
      window_label_->GetNativeWindow()->SetBounds(label_bounds);
    }
  }
}

void WindowSelectorItem::CreateWindowLabel(const base::string16& title) {
  window_label_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window_, ash::kShellWindowId_OverlayContainer);
  params.accept_events = false;
  params.visible_on_all_workspaces = true;
  window_label_->set_focus_on_creation(false);
  window_label_->Init(params);
  window_label_view_ = new views::Label;
  window_label_view_->SetEnabledColor(kLabelColor);
  window_label_view_->SetBackgroundColor(kLabelBackground);
  window_label_view_->SetShadows(gfx::ShadowValues(
      1,
      gfx::ShadowValue(
          gfx::Point(0, kVerticalShadowOffset), kShadowBlur, kLabelShadow)));
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  window_label_view_->SetFontList(
      bundle.GetFontList(ui::ResourceBundle::BoldFont));
  window_label_view_->SetText(title);
  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0,
                                                  kVerticalLabelPadding,
                                                  0);
  window_label_view_->SetLayoutManager(layout);
  window_label_->SetContentsView(window_label_view_);
  window_label_->Show();
}

}  // namespace ash
