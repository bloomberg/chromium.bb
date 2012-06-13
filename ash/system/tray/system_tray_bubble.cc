// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_bubble.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/window_animations.h"
#include "base/message_loop.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

const int kAnimationDurationForPopupMS = 200;

// Normally a detailed view is the same size as the default view. However,
// when showing a detailed view directly (e.g. clicking on a notification),
// we may not know the height of the default view, or the default view may
// be too short, so we use this as a default and minimum height for any
// detailed view.
const int kDetailedBubbleMaxHeight = kTrayPopupItemHeight * 5;

// A view with some special behaviour for tray items in the popup:
// - changes background color on hover.
class TrayPopupItemContainer : public views::View {
 public:
  explicit TrayPopupItemContainer(views::View* view) : hover_(false) {
    set_notify_enter_exit_on_child(true);
    set_border(view->border() ? views::Border::CreateEmptyBorder(0, 0, 0, 0) :
        views::Border::CreateSolidSidedBorder(1, 1, 0, 1, kBorderDarkColor));
    views::BoxLayout* layout = new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 0);
    layout->set_spread_blank_space(true);
    SetLayoutManager(layout);
    SetPaintToLayer(view->layer() != NULL);
    if (view->layer())
      SetFillsBoundsOpaquely(view->layer()->fills_bounds_opaquely());
    AddChildView(view);
    SetVisible(view->visible());
  }

  virtual ~TrayPopupItemContainer() {}

 private:
  // Overridden from views::View.
  virtual void ChildVisibilityChanged(View* child) OVERRIDE {
    if (visible() == child->visible())
      return;
    SetVisible(child->visible());
    PreferredSizeChanged();
  }

  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE {
    PreferredSizeChanged();
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    hover_ = true;
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    hover_ = false;
    SchedulePaint();
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    if (child_count() == 0)
      return;

    views::View* view = child_at(0);
    if (!view->background()) {
      canvas->FillRect(gfx::Rect(size()),
          hover_ ? kHoverBackgroundColor : kBackgroundColor);
    }
  }

  bool hover_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupItemContainer);
};

// Implicit animation observer that deletes itself and the layer at the end of
// the animation.
class AnimationObserverDeleteLayer : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverDeleteLayer(ui::Layer* layer)
      : layer_(layer) {
  }

  virtual ~AnimationObserverDeleteLayer() {
  }

  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    MessageLoopForUI::current()->DeleteSoon(FROM_HERE, this);
  }

 private:
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserverDeleteLayer);
};

}  // namespace

namespace internal {

// SystemTrayBubble::InitParams
SystemTrayBubble::InitParams::InitParams(
    SystemTrayBubble::AnchorType anchor_type,
    ShelfAlignment shelf_alignment)
    : anchor(NULL),
      anchor_type(anchor_type),
      can_activate(false),
      login_status(ash::user::LOGGED_IN_NONE),
      arrow_offset(0),
      max_height(0) {
}

// SystemTrayBubble

SystemTrayBubble::SystemTrayBubble(
    ash::SystemTray* tray,
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type)
    : tray_(tray),
      bubble_view_(NULL),
      bubble_widget_(NULL),
      items_(items),
      bubble_type_(bubble_type),
      anchor_type_(ANCHOR_TYPE_TRAY),
      autoclose_delay_(0) {
  if (bubble_type_ != BUBBLE_TYPE_NOTIFICATION)
    Shell::GetInstance()->AddEnvEventFilter(this);
}

SystemTrayBubble::~SystemTrayBubble() {
  if (bubble_type_ != BUBBLE_TYPE_NOTIFICATION)
    Shell::GetInstance()->RemoveEnvEventFilter(this);

  DestroyItemViews();
  // Reset the host pointer in bubble_view_ in case its destruction is deferred.
  if (bubble_view_)
    bubble_view_->reset_host();
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    // This triggers the destruction of bubble_view_.
    bubble_widget_->Close();
  }
}

void SystemTrayBubble::UpdateView(
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type) {
  DCHECK(bubble_type != BUBBLE_TYPE_NOTIFICATION);
  DCHECK(bubble_type != bubble_type_);

  const int kSwipeDelayMS = 300;
  base::TimeDelta swipe_duration =
      base::TimeDelta::FromMilliseconds(kSwipeDelayMS);
  ui::Layer* layer = bubble_view_->RecreateLayer();
  DCHECK(layer);
  layer->SuppressPaint();

  // When transitioning from detailed view to default view, animate the existing
  // view (slide out towards the right).
  if (bubble_type == BUBBLE_TYPE_DEFAULT) {
    // Make sure the old view is visibile over the new view during the
    // animation.
    layer->parent()->StackAbove(layer, bubble_view_->layer());
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.AddObserver(new AnimationObserverDeleteLayer(layer));
    settings.SetTransitionDuration(swipe_duration);
    settings.SetTweenType(ui::Tween::EASE_IN);
    ui::Transform transform;
    transform.SetTranslateX(layer->bounds().width());
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
      // Animate the darkening effect a little longer than the swipe-in. This is
      // to make sure the darkening animation does not end up finishing early,
      // because the dark layer goes away at the end of the animation, and there
      // is a brief moment when the old view is still visible, but it does not
      // have the shadow layer on top.
      ui::ScopedLayerAnimationSettings settings(shadow->GetAnimator());
      settings.AddObserver(new AnimationObserverDeleteLayer(shadow));
      settings.SetTransitionDuration(swipe_duration +
                                     base::TimeDelta::FromMilliseconds(150));
      settings.SetTweenType(ui::Tween::LINEAR);
      shadow->SetOpacity(0.15f);
    }
  }

  DestroyItemViews();
  bubble_view_->RemoveAllChildViews(true);

  items_ = items;
  bubble_type_ = bubble_type;
  CreateItemViews(Shell::GetInstance()->tray_delegate()->GetUserLoginStatus());
  bubble_widget_->GetContentsView()->Layout();
  // Make sure that the bubble is large enough for the default view.
  if (bubble_type_ == BUBBLE_TYPE_DEFAULT) {
    bubble_view_->SetMaxHeight(0);  // Clear max height limit.
  }

  // When transitioning from default view to detailed view, animate the new
  // view (slide in from the right).
  if (bubble_type == BUBBLE_TYPE_DETAILED) {
    ui::Layer* new_layer = bubble_view_->layer();
    gfx::Rect bounds = new_layer->bounds();
    ui::Transform transform;
    transform.SetTranslateX(bounds.width());
    new_layer->SetTransform(transform);
    {
      ui::ScopedLayerAnimationSettings settings(new_layer->GetAnimator());
      settings.AddObserver(new AnimationObserverDeleteLayer(layer));
      settings.SetTransitionDuration(swipe_duration);
      settings.SetTweenType(ui::Tween::EASE_IN);
      new_layer->SetTransform(ui::Transform());
    }
  }
}

void SystemTrayBubble::InitView(const InitParams& init_params) {
  DCHECK(bubble_view_ == NULL);
  anchor_type_ = init_params.anchor_type;
  views::BubbleBorder::ArrowLocation arrow_location;
  if (anchor_type_ == ANCHOR_TYPE_TRAY) {
    if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
      arrow_location = views::BubbleBorder::BOTTOM_RIGHT;
    } else if (tray_->shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
      arrow_location = views::BubbleBorder::LEFT_BOTTOM;
    } else {
      arrow_location = views::BubbleBorder::RIGHT_BOTTOM;
    }
  } else {
    arrow_location = views::BubbleBorder::NONE;
  }
  bubble_view_ = new TrayBubbleView(
      init_params.anchor, arrow_location,
      this, init_params.can_activate, kTrayPopupWidth);
  if (bubble_type_ == BUBBLE_TYPE_NOTIFICATION)
    bubble_view_->set_close_on_deactivate(false);
  int max_height = init_params.max_height;
  if (bubble_type_ == BUBBLE_TYPE_DETAILED &&
      max_height < kDetailedBubbleMaxHeight)
    max_height = kDetailedBubbleMaxHeight;
  bubble_view_->SetMaxHeight(max_height);

  CreateItemViews(init_params.login_status);

  DCHECK(bubble_widget_ == NULL);
  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);

  // Must occur after call to CreateBubble()
  bubble_view_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_widget_->non_client_view()->frame_view()->set_background(NULL);
  bubble_view_->SetBubbleBorder(init_params.arrow_offset);

  bubble_widget_->AddObserver(this);

  // Setup animation.
  ash::SetWindowVisibilityAnimationType(
      bubble_widget_->GetNativeWindow(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ash::SetWindowVisibilityAnimationTransition(
      bubble_widget_->GetNativeWindow(),
      ash::ANIMATE_BOTH);
  ash::SetWindowVisibilityAnimationDuration(
      bubble_widget_->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));

  bubble_view_->Show();
}

void SystemTrayBubble::BubbleViewDestroyed() {
  DestroyItemViews();
}

gfx::Rect SystemTrayBubble::GetAnchorRect() const {
  gfx::Rect rect;
  views::Widget* widget = bubble_view()->anchor_widget();
  if (widget->IsVisible()) {
    rect = widget->GetWindowScreenBounds();
    if (anchor_type_ == ANCHOR_TYPE_TRAY) {
      if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
        rect.Inset(
            base::i18n::IsRTL() ?
                kPaddingFromRightEdgeOfScreenBottomAlignment : 0,
            0,
            base::i18n::IsRTL() ?
                0 : kPaddingFromRightEdgeOfScreenBottomAlignment,
            kPaddingFromBottomOfScreenBottomAlignment);
      } else if (tray_->shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
        rect.Inset(0, 0, kPaddingFromEdgeOfLauncherVerticalAlignment,
                   kPaddingFromBottomOfScreenVerticalAlignment);
      } else {
        rect.Inset(kPaddingFromEdgeOfLauncherVerticalAlignment + 4,
                   0, 0, kPaddingFromBottomOfScreenVerticalAlignment);
      }
    } else if (anchor_type_ == ANCHOR_TYPE_BUBBLE) {
      // For notification bubble to be anchored with uber tray bubble,
      // the anchor can include arrow on left or right, which should
      // be deducted out from the anchor rect.
      views::View* anchor_view = bubble_view()->anchor_view();
      rect = anchor_view->GetScreenBounds();
      gfx::Insets insets = anchor_view->GetInsets();
      rect.Inset(insets);
    }
  }
  return rect;
}

void SystemTrayBubble::OnMouseEnteredView() {
  StopAutoCloseTimer();
}

void SystemTrayBubble::OnMouseExitedView() {
  RestartAutoCloseTimer();
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
  if (bubble_widget_)
    bubble_widget_->Close();
}

void SystemTrayBubble::CreateItemViews(user::LoginStatus login_status) {
  for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    views::View* view = NULL;
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        view = (*it)->CreateDefaultView(login_status);
        break;
      case BUBBLE_TYPE_DETAILED:
        view = (*it)->CreateDetailedView(login_status);
        break;
      case BUBBLE_TYPE_NOTIFICATION:
        view = (*it)->CreateNotificationView(login_status);
        break;
    }
    if (view)
      bubble_view_->AddChildView(new TrayPopupItemContainer(view));
  }
}

bool SystemTrayBubble::ProcessLocatedEvent(const aura::LocatedEvent& event) {
  DCHECK_NE(BUBBLE_TYPE_NOTIFICATION, bubble_type_);
  gfx::Rect bounds = bubble_widget_->GetNativeWindow()->GetBoundsInRootWindow();
  if (bounds.Contains(event.root_location()))
    return false;

  bubble_widget_->Close();

  // If the user clicks on the tray while the bubble is up, then the bubble will
  // close. But after the mouse-up event on the tray, the bubble will show up
  // again. To prevent this from happening, if this mouse-press event that
  // closed the bubble is on the system tray, then do not let the event reach
  // the tray.
  bounds = tray_->GetWidget()->GetNativeWindow()->GetBoundsInRootWindow();
  if (bounds.Contains(event.root_location()))
    return true;
  return false;
}

bool SystemTrayBubble::PreHandleKeyEvent(aura::Window* target,
                                         aura::KeyEvent* event) {
  return false;
}

bool SystemTrayBubble::PreHandleMouseEvent(aura::Window* target,
                                           aura::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    return ProcessLocatedEvent(*event);
  return false;
}

ui::TouchStatus SystemTrayBubble::PreHandleTouchEvent(aura::Window* target,
                                                      aura::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED && ProcessLocatedEvent(*event))
    return ui::TOUCH_STATUS_END;
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus SystemTrayBubble::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

void SystemTrayBubble::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_ = NULL;
  tray_->RemoveBubble(this);
}

}  // namespace internal
}  // namespace ash
