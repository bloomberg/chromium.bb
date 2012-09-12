// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_bubble.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "base/message_loop.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Normally a detailed view is the same size as the default view. However,
// when showing a detailed view directly (e.g. clicking on a notification),
// we may not know the height of the default view, or the default view may
// be too short, so we use this as a default and minimum height for any
// detailed view.
const int kDetailedBubbleMaxHeight = kTrayPopupItemHeight * 5;

// TODO(stevenjb/jennyz): Remove this when TrayBubbleBorder is integrated with
// BubbleBorder. See crbug.com/132772, crbug.com/139813.
class TrayPopupItemBorder : public views::Border {
 public:
  explicit TrayPopupItemBorder(views::View* owner, ShelfAlignment alignment)
      : owner_(owner),
        alignment_(alignment) {
  }
  virtual ~TrayPopupItemBorder() {}

 private:
  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    const views::View* parent = view.parent();
    int index = parent->GetIndexOf(&view);

    // Draw a dark top-border for the first item.
    if (index == 0)
      canvas->FillRect(gfx::Rect(0, 0, view.width(), 1), kBorderDarkColor);

    // Bottom border.
    if ((index != parent->child_count() - 1) ||
        (alignment_ != SHELF_ALIGNMENT_BOTTOM)) {
      canvas->FillRect(gfx::Rect(0, view.height() - 1, view.width(), 1),
                       kBorderLightColor);
    }

    // Left and right borders.
    if (alignment_ != SHELF_ALIGNMENT_LEFT) {
      canvas->FillRect(gfx::Rect(0, 0, 1, view.height()),
                       kBorderDarkColor);
    }
    if (alignment_ != SHELF_ALIGNMENT_RIGHT) {
      canvas->FillRect(gfx::Rect(view.width() - 1, 0, 1, view.height()),
                       kBorderDarkColor);
    }
  }

  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    const views::View* parent = owner_->parent();
    int index = parent->GetIndexOf(owner_);
    int left = (alignment_ == SHELF_ALIGNMENT_LEFT) ? 0 : 1;
    int right = (alignment_ == SHELF_ALIGNMENT_RIGHT) ? 0 : 1;
    insets->Set(index == 0 ? 1 : 0,
                left,
                (index != parent->child_count() - 1) ? 1 : 0,
                right);
  }

  views::View* owner_;
  ShelfAlignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupItemBorder);
};

// A view with some special behaviour for tray items in the popup:
// - optionally changes background color on hover.
class TrayPopupItemContainer : public views::View {
 public:
  TrayPopupItemContainer(views::View* view,
                         ShelfAlignment alignment,
                         bool change_background)
      : hover_(false),
        change_background_(change_background) {
    set_notify_enter_exit_on_child(true);
    set_border(new TrayPopupItemBorder(this, alignment));
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

  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE {
    hover_ = true;
    SchedulePaint();
  }

  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE {
    hover_ = false;
    SchedulePaint();
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    if (child_count() == 0)
      return;

    views::View* view = child_at(0);
    if (!view->background()) {
      canvas->FillRect(gfx::Rect(size()), (hover_ && change_background_) ?
          kHoverBackgroundColor : kBackgroundColor);
    }
  }

  bool hover_;
  bool change_background_;

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
      autoclose_delay_(0) {
}

SystemTrayBubble::~SystemTrayBubble() {
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

  const int kSwipeDelayMS = 150;
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
    settings.SetTweenType(ui::Tween::EASE_OUT);
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

  // Close bubble view if we failed to create the item view.
  if (!bubble_view_->has_children()) {
    Close();
    return;
  }

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
      settings.SetTweenType(ui::Tween::EASE_OUT);
      new_layer->SetTransform(ui::Transform());
    }
  }
}

void SystemTrayBubble::InitView(views::View* anchor,
                                TrayBubbleView::InitParams init_params,
                                user::LoginStatus login_status) {
  DCHECK(bubble_view_ == NULL);

  if (bubble_type_ == BUBBLE_TYPE_DETAILED &&
      init_params.max_height < kDetailedBubbleMaxHeight) {
    init_params.max_height = kDetailedBubbleMaxHeight;
  } else if (bubble_type_ == BUBBLE_TYPE_NOTIFICATION) {
    init_params.close_on_deactivate = false;
  }
  bubble_view_ = TrayBubbleView::Create(anchor, this, init_params);

  CreateItemViews(login_status);

  DCHECK(bubble_widget_ == NULL);
  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

  InitializeAndShowBubble(bubble_widget_, bubble_view_, tray_);
}

void SystemTrayBubble::BubbleViewDestroyed() {
  DestroyItemViews();
  bubble_view_ = NULL;
}

void SystemTrayBubble::OnMouseEnteredView() {
  StopAutoCloseTimer();
  tray_->UpdateShouldShowLauncher();
}

void SystemTrayBubble::OnMouseExitedView() {
  RestartAutoCloseTimer();
  tray_->UpdateShouldShowLauncher();
}

void SystemTrayBubble::OnClickedOutsideView() {
  if (bubble_type_ != BUBBLE_TYPE_NOTIFICATION)
    bubble_widget_->Close();
}

string16 SystemTrayBubble::GetAccessibleName() {
  return tray_->GetAccessibleName();
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

void SystemTrayBubble::SetVisible(bool is_visible) {
  if (!bubble_widget_)
    return;
  if (is_visible)
    bubble_widget_->Show();
  else
    bubble_widget_->Hide();
}

bool SystemTrayBubble::IsVisible() {
  return bubble_widget_ && bubble_widget_->IsVisible();
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
    if (view) {
      bubble_view_->AddChildView(new TrayPopupItemContainer(
          view, tray_->shelf_alignment(), bubble_type_ == BUBBLE_TYPE_DEFAULT));
    }
  }
}

void SystemTrayBubble::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_ = NULL;
  tray_->RemoveBubble(this);
}

}  // namespace internal
}  // namespace ash
