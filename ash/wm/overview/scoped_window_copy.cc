// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_window_copy.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/display.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Creates a copy of |window| with |recreated_layer| in the |target_root|.
views::Widget* CreateCopyOfWindow(aura::Window* target_root,
                                  aura::Window* src_window,
                                  ui::Layer* recreated_layer) {
  // Save and remove the transform from the layer to later reapply to both the
  // source and newly created copy window.
  gfx::Transform transform = recreated_layer->transform();
  recreated_layer->SetTransform(gfx::Transform());

  src_window->SetTransform(transform);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = src_window->parent();
  params.keep_on_top = true;
  widget->set_focus_on_creation(false);
  widget->Init(params);
  widget->SetVisibilityChangedAnimationsEnabled(false);
  std::string name = src_window->name() + " (Copy)";
  widget->GetNativeWindow()->SetName(name);
  ::wm::SetShadowType(widget->GetNativeWindow(),
                               ::wm::SHADOW_TYPE_RECTANGULAR);

  // Set the bounds in the target root window.
  gfx::Display target_display =
      Shell::GetScreen()->GetDisplayNearestWindow(target_root);
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(src_window->GetRootWindow());
  if (screen_position_client && target_display.is_valid()) {
    screen_position_client->SetBounds(widget->GetNativeWindow(),
        src_window->GetBoundsInScreen(), target_display);
  } else {
    widget->SetBounds(src_window->GetBoundsInScreen());
  }
  widget->StackAbove(src_window);

  // Move the |recreated_layer| to the newly created window.
  recreated_layer->set_delegate(src_window->layer()->delegate());
  gfx::Rect layer_bounds = recreated_layer->bounds();
  layer_bounds.set_origin(gfx::Point(0, 0));
  recreated_layer->SetBounds(layer_bounds);
  recreated_layer->SetVisible(false);
  recreated_layer->parent()->Remove(recreated_layer);

  aura::Window* window = widget->GetNativeWindow();
  recreated_layer->SetVisible(true);
  window->layer()->Add(recreated_layer);
  window->layer()->StackAtTop(recreated_layer);
  window->layer()->SetOpacity(1);
  window->SetTransform(transform);
  window->Show();
  return widget;
}

}  // namespace

// An observer which closes the widget and deletes the layer after an
// animation finishes.
class CleanupWidgetAfterAnimationObserver : public ui::LayerAnimationObserver {
 public:
  CleanupWidgetAfterAnimationObserver(
      views::Widget* widget,
      scoped_ptr<ui::LayerTreeOwner> layer_owner);

  // Takes ownership of the widget. At this point the class will delete itself
  // and clean up the layer when there are no pending animations.
  void TakeOwnershipOfWidget();

  // ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE;

 private:
  virtual ~CleanupWidgetAfterAnimationObserver();

  // If the necessary conditions have been satisfied to destruct this
  // class, deletes itself and cleans up the widget and layer.
  void MaybeDestruct();

  views::Widget* widget_;
  scoped_ptr<ui::LayerTreeOwner> layer_owner_;
  bool owns_widget_;
  int pending_animations_;

  DISALLOW_COPY_AND_ASSIGN(CleanupWidgetAfterAnimationObserver);
};

CleanupWidgetAfterAnimationObserver::CleanupWidgetAfterAnimationObserver(
        views::Widget* widget,
        scoped_ptr<ui::LayerTreeOwner> layer_owner)
    : widget_(widget),
      layer_owner_(layer_owner.Pass()),
      owns_widget_(false),
      pending_animations_(0) {
  widget_->GetNativeWindow()->layer()->GetAnimator()->AddObserver(this);
}

void CleanupWidgetAfterAnimationObserver::TakeOwnershipOfWidget() {
  owns_widget_ = true;
  MaybeDestruct();
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  pending_animations_--;
  MaybeDestruct();
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  pending_animations_--;
  MaybeDestruct();
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  pending_animations_++;
}

CleanupWidgetAfterAnimationObserver::~CleanupWidgetAfterAnimationObserver() {
  widget_->GetNativeWindow()->layer()->GetAnimator()->RemoveObserver(this);
  widget_->Close();
  widget_ = NULL;
}

void CleanupWidgetAfterAnimationObserver::MaybeDestruct() {
  if (pending_animations_ || !owns_widget_)
    return;
  delete this;
}

ScopedWindowCopy::ScopedWindowCopy(aura::Window* target_root,
                                   aura::Window* src_window) {
  scoped_ptr<ui::LayerTreeOwner> layer_owner =
      ::wm::RecreateLayers(src_window);
  widget_ = CreateCopyOfWindow(target_root, src_window, layer_owner->root());
  cleanup_observer_ =
      new CleanupWidgetAfterAnimationObserver(widget_, layer_owner.Pass());
}

ScopedWindowCopy::~ScopedWindowCopy() {
  // The cleanup observer will delete itself and the window when any pending
  // animations have completed.
  cleanup_observer_->TakeOwnershipOfWidget();
}

aura::Window* ScopedWindowCopy::GetWindow() {
  return widget_->GetNativeWindow();
}

}  // namespace ash
