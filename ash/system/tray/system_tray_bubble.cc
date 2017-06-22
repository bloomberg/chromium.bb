// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_bubble.h"

#include <utility>
#include <vector>

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using views::TrayBubbleView;

namespace ash {

namespace {

// Normally a detailed view is the same size as the default view. However,
// when showing a detailed view directly (e.g. clicking on a notification),
// we may not know the height of the default view, or the default view may
// be too short, so we use this as a default and minimum height for any
// detailed view.
int GetDetailedBubbleMaxHeight() {
  return kTrayPopupItemMinHeight * 5;
}

// Duration of swipe animation used when transitioning from a default to
// detailed view or vice versa.
const int kSwipeDelayMS = 150;

// Extra bottom padding when showing the BUBBLE_TYPE_DEFAULT view.
const int kDefaultViewBottomPadding = 4;

// Implicit animation observer that deletes itself and the layer at the end of
// the animation.
class AnimationObserverDeleteLayer : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverDeleteLayer(ui::Layer* layer) : layer_(layer) {}

  ~AnimationObserverDeleteLayer() override {}

  void OnImplicitAnimationsCompleted() override {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  std::unique_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserverDeleteLayer);
};

}  // namespace

// SystemTrayBubble

SystemTrayBubble::SystemTrayBubble(
    ash::SystemTray* tray,
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type)
    : tray_(tray),
      bubble_view_(nullptr),
      items_(items),
      bubble_type_(bubble_type),
      autoclose_delay_(0) {}

SystemTrayBubble::~SystemTrayBubble() {
  DestroyItemViews();
  // Reset the host pointer in bubble_view_ in case its destruction is deferred.
  if (bubble_view_)
    bubble_view_->ResetDelegate();
}

void SystemTrayBubble::UpdateView(
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type) {
  std::unique_ptr<ui::Layer> scoped_layer;
  if (bubble_type != bubble_type_) {
    base::TimeDelta swipe_duration =
        base::TimeDelta::FromMilliseconds(kSwipeDelayMS);
    scoped_layer = bubble_view_->RecreateLayer();
    // Keep the reference to layer as we need it after releasing it.
    ui::Layer* layer = scoped_layer.get();
    DCHECK(layer);
    layer->SuppressPaint();

    // When transitioning from detailed view to default view, animate the
    // existing view (slide out towards the right).
    if (bubble_type == BUBBLE_TYPE_DEFAULT) {
      ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
      settings.AddObserver(
          new AnimationObserverDeleteLayer(scoped_layer.release()));
      settings.SetTransitionDuration(swipe_duration);
      settings.SetTweenType(gfx::Tween::EASE_OUT);
      gfx::Transform transform;
      transform.Translate(layer->bounds().width(), 0.0);
      layer->SetTransform(transform);
    }

    {
      // Add a shadow layer to make the old layer darker as the animation
      // progresses.
      ui::Layer* shadow = new ui::Layer(ui::LAYER_SOLID_COLOR);
      shadow->SetColor(SK_ColorBLACK);
      shadow->SetOpacity(0.01f);
      shadow->SetBounds(layer->bounds());
      layer->Add(shadow);
      layer->StackAtTop(shadow);
      {
        // Animate the darkening effect a little longer than the swipe-in. This
        // is to make sure the darkening animation does not end up finishing
        // early, because the dark layer goes away at the end of the animation,
        // and there is a brief moment when the old view is still visible, but
        // it does not have the shadow layer on top.
        ui::ScopedLayerAnimationSettings settings(shadow->GetAnimator());
        settings.AddObserver(new AnimationObserverDeleteLayer(shadow));
        settings.SetTransitionDuration(swipe_duration +
                                       base::TimeDelta::FromMilliseconds(150));
        settings.SetTweenType(gfx::Tween::LINEAR);
        shadow->SetOpacity(0.15f);
      }
    }
  }

  DestroyItemViews();
  bubble_view_->RemoveAllChildViews(true);

  items_ = items;
  bubble_type_ = bubble_type;
  CreateItemViews(Shell::Get()->session_controller()->login_status());

  // Close bubble view if we failed to create the item view.
  if (!bubble_view_->has_children()) {
    Close();
    return;
  }

  UpdateBottomPadding();
  bubble_view_->GetWidget()->GetContentsView()->Layout();
  // Make sure that the bubble is large enough for the default view.
  if (bubble_type_ == BUBBLE_TYPE_DEFAULT) {
    bubble_view_->SetMaxHeight(0);  // Clear max height limit.
  }

  if (scoped_layer) {
    // When transitioning from default view to detailed view, animate the new
    // view (slide in from the right).
    if (bubble_type == BUBBLE_TYPE_DETAILED) {
      ui::Layer* new_layer = bubble_view_->layer();

      // Make sure the new layer is stacked above the old layer during the
      // animation.
      new_layer->parent()->StackAbove(new_layer, scoped_layer.get());

      gfx::Rect bounds = new_layer->bounds();
      gfx::Transform transform;
      transform.Translate(bounds.width(), 0.0);
      new_layer->SetTransform(transform);
      {
        ui::ScopedLayerAnimationSettings settings(new_layer->GetAnimator());
        settings.AddObserver(
            new AnimationObserverDeleteLayer(scoped_layer.release()));
        settings.SetTransitionDuration(
            base::TimeDelta::FromMilliseconds(kSwipeDelayMS));
        settings.SetTweenType(gfx::Tween::EASE_OUT);
        new_layer->SetTransform(gfx::Transform());
      }
    }
  }
}

void SystemTrayBubble::InitView(views::View* anchor,
                                LoginStatus login_status,
                                TrayBubbleView::InitParams* init_params) {
  DCHECK(anchor);
  DCHECK(!bubble_view_);

  if (bubble_type_ == BUBBLE_TYPE_DETAILED &&
      init_params->max_height < GetDetailedBubbleMaxHeight()) {
    init_params->max_height = GetDetailedBubbleMaxHeight();
  }

  init_params->delegate = tray_;
  // Place the bubble on same display as this system tray.
  init_params->parent_window = tray_->GetBubbleWindowContainer();
  init_params->anchor_view = anchor;
  bubble_view_ = new TrayBubbleView(*init_params);
  UpdateBottomPadding();
  bubble_view_->set_adjust_if_offscreen(false);
  CreateItemViews(login_status);

  if (bubble_view_->CanActivate()) {
    bubble_view_->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  }
}

void SystemTrayBubble::DestroyItemViews() {
  for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end(); ++it) {
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        (*it)->OnDefaultViewDestroyed();
        break;
      case BUBBLE_TYPE_DETAILED:
        (*it)->OnDetailedViewDestroyed();
        break;
    }
  }
}

void SystemTrayBubble::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
}

void SystemTrayBubble::StartAutoCloseTimer(int seconds) {
  autoclose_.Stop();
  autoclose_delay_ = seconds;
  if (autoclose_delay_) {
    autoclose_.Start(FROM_HERE, base::TimeDelta::FromSeconds(autoclose_delay_),
                     this, &SystemTrayBubble::Close);
  }
}

void SystemTrayBubble::StopAutoCloseTimer() {
  autoclose_.Stop();
}

void SystemTrayBubble::RestartAutoCloseTimer() {
  if (autoclose_delay_)
    StartAutoCloseTimer(autoclose_delay_);
}

void SystemTrayBubble::Close() {
  tray_->HideBubbleWithView(bubble_view());
}

void SystemTrayBubble::SetVisible(bool is_visible) {
  if (!bubble_view_)
    return;
  views::Widget* bubble_widget = bubble_view_->GetWidget();
  if (is_visible)
    bubble_widget->Show();
  else
    bubble_widget->Hide();
}

bool SystemTrayBubble::IsVisible() {
  return bubble_view() && bubble_view()->GetWidget()->IsVisible();
}

bool SystemTrayBubble::ShouldShowShelf() const {
  for (std::vector<ash::SystemTrayItem*>::const_iterator it = items_.begin();
       it != items_.end(); ++it) {
    if ((*it)->ShouldShowShelf())
      return true;
  }
  return false;
}

void SystemTrayBubble::RecordVisibleRowMetrics() {
  if (bubble_type_ != BUBBLE_TYPE_DEFAULT)
    return;

  for (const std::pair<SystemTrayItem::UmaType, views::View*>& pair :
       tray_item_view_map_) {
    if (pair.second->visible() &&
        pair.first != SystemTrayItem::UMA_NOT_RECORDED) {
      UMA_HISTOGRAM_ENUMERATION("Ash.SystemMenu.DefaultView.VisibleRows",
                                pair.first, SystemTrayItem::UMA_COUNT);
    }
  }
}

void SystemTrayBubble::UpdateBottomPadding() {
  if (bubble_type_ == BUBBLE_TYPE_DEFAULT)
    bubble_view_->SetBottomPadding(kDefaultViewBottomPadding);
  else
    bubble_view_->SetBottomPadding(0);
}

void SystemTrayBubble::CreateItemViews(LoginStatus login_status) {
  tray_item_view_map_.clear();

  // If a system modal dialog is present, create the same tray as
  // in locked state.
  if (ShellPort::Get()->IsSystemModalWindowOpen() &&
      login_status != LoginStatus::NOT_LOGGED_IN) {
    login_status = LoginStatus::LOCKED;
  }

  views::View* focus_view = nullptr;
  for (size_t i = 0; i < items_.size(); ++i) {
    views::View* item_view = nullptr;
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        item_view = items_[i]->CreateDefaultView(login_status);
        if (items_[i]->restore_focus())
          focus_view = item_view;
        break;
      case BUBBLE_TYPE_DETAILED:
        item_view = items_[i]->CreateDetailedView(login_status);
        break;
    }
    if (item_view) {
      bubble_view_->AddChildView(item_view);
      tray_item_view_map_[items_[i]->uma_type()] = item_view;
    }
  }

  if (focus_view) {
    tray_->ActivateBubble();
    focus_view->RequestFocus();
  }
}

}  // namespace ash
