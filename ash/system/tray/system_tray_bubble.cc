// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_bubble.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_container.h"
#include "base/thread_task_runner_handle.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
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
const int kDetailedBubbleMaxHeight = kTrayPopupItemHeight * 5;

// Duration of swipe animation used when transitioning from a default to
// detailed view or vice versa.
const int kSwipeDelayMS = 150;

// Implicit animation observer that deletes itself and the layer at the end of
// the animation.
class AnimationObserverDeleteLayer : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverDeleteLayer(ui::Layer* layer)
      : layer_(layer) {
  }

  ~AnimationObserverDeleteLayer() override {}

  void OnImplicitAnimationsCompleted() override {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserverDeleteLayer);
};

}  // namespace

// SystemTrayBubble

SystemTrayBubble::SystemTrayBubble(
    ash::SystemTray* tray,
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type)
    : tray_(tray),
      bubble_view_(NULL),
      items_(items),
      bubble_type_(bubble_type),
      autoclose_delay_(0) {
}

SystemTrayBubble::~SystemTrayBubble() {
  DestroyItemViews();
  // Reset the host pointer in bubble_view_ in case its destruction is deferred.
  if (bubble_view_)
    bubble_view_->reset_delegate();
}

void SystemTrayBubble::UpdateView(
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type) {
  DCHECK(bubble_type != BUBBLE_TYPE_NOTIFICATION);

  scoped_ptr<ui::Layer> scoped_layer;
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
  CreateItemViews(
      Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus());

  // Close bubble view if we failed to create the item view.
  if (!bubble_view_->has_children()) {
    Close();
    return;
  }

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
                                user::LoginStatus login_status,
                                TrayBubbleView::InitParams* init_params) {
  DCHECK(bubble_view_ == NULL);

  if (bubble_type_ == BUBBLE_TYPE_DETAILED &&
      init_params->max_height < kDetailedBubbleMaxHeight) {
    init_params->max_height = kDetailedBubbleMaxHeight;
  } else if (bubble_type_ == BUBBLE_TYPE_NOTIFICATION) {
    init_params->close_on_deactivate = false;
  }
  bubble_view_ = TrayBubbleView::Create(
      tray_->GetBubbleWindowContainer(), anchor, tray_, init_params);
  bubble_view_->set_adjust_if_offscreen(false);
  CreateItemViews(login_status);

  if (bubble_view_->CanActivate()) {
    bubble_view_->NotifyAccessibilityEvent(
        ui::AX_EVENT_ALERT, true);
  }
}

void SystemTrayBubble::FocusDefaultIfNeeded() {
  views::FocusManager* manager = bubble_view_->GetFocusManager();
  if (!manager || manager->GetFocusedView())
    return;

  views::View* view = manager->GetNextFocusableView(NULL, NULL, false, false);
  if (view)
    view->RequestFocus();
}

void SystemTrayBubble::DestroyItemViews() {
  for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        (*it)->DestroyDefaultView();
        break;
      case BUBBLE_TYPE_DETAILED:
        (*it)->DestroyDetailedView();
        break;
      case BUBBLE_TYPE_NOTIFICATION:
        (*it)->DestroyNotificationView();
        break;
    }
  }
}

void SystemTrayBubble::BubbleViewDestroyed() {
  bubble_view_ = NULL;
}

void SystemTrayBubble::StartAutoCloseTimer(int seconds) {
  autoclose_.Stop();
  autoclose_delay_ = seconds;
  if (autoclose_delay_) {
    autoclose_.Start(FROM_HERE,
                     base::TimeDelta::FromSeconds(autoclose_delay_),
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
       it != items_.end();
       ++it) {
    if ((*it)->ShouldShowShelf())
      return true;
  }
  return false;
}

void SystemTrayBubble::CreateItemViews(user::LoginStatus login_status) {
  std::vector<views::View*> item_views;
  // If a system modal dialog is present, create the same tray as
  // in locked state.
  if (Shell::GetInstance()->IsSystemModalWindowOpen() &&
      login_status != user::LOGGED_IN_NONE) {
    login_status = user::LOGGED_IN_LOCKED;
  }

  views::View* focus_view = NULL;
  for (size_t i = 0; i < items_.size(); ++i) {
    views::View* view = NULL;
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        view = items_[i]->CreateDefaultView(login_status);
        if (items_[i]->restore_focus())
          focus_view = view;
        break;
      case BUBBLE_TYPE_DETAILED:
        view = items_[i]->CreateDetailedView(login_status);
        break;
      case BUBBLE_TYPE_NOTIFICATION:
        view = items_[i]->CreateNotificationView(login_status);
        break;
    }
    if (view)
      item_views.push_back(view);
  }

  bool is_default_bubble = bubble_type_ == BUBBLE_TYPE_DEFAULT;
  for (size_t i = 0; i < item_views.size(); ++i) {
    // For default view, draw bottom border for each item, except the last
    // 2 items, which are the bottom header row and the one just above it.
    bubble_view_->AddChildView(new TrayPopupItemContainer(
        item_views[i], is_default_bubble,
        is_default_bubble && (i < item_views.size() - 2)));
  }
  if (focus_view)
    focus_view->RequestFocus();
}

}  // namespace ash
