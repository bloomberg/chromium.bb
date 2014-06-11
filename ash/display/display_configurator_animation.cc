// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_configurator_animation.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace {

const int kFadingAnimationDurationInMS = 200;
const int kFadingTimeoutDurationInSeconds = 10;

// CallbackRunningObserver accepts multiple layer animations and
// runs the specified |callback| when all of the animations have finished.
class CallbackRunningObserver {
 public:
  CallbackRunningObserver(base::Closure callback)
      : completed_counter_(0),
        animation_aborted_(false),
        callback_(callback) {}

  void AddNewAnimator(ui::LayerAnimator* animator) {
    Observer* observer = new Observer(animator, this);
    animator->AddObserver(observer);
    observer_list_.push_back(observer);
  }

 private:
  void OnSingleTaskCompleted() {
    completed_counter_++;
    if (completed_counter_ >= observer_list_.size()) {
      base::MessageLoopForUI::current()->DeleteSoon(FROM_HERE, this);
      if (!animation_aborted_)
        base::MessageLoopForUI::current()->PostTask(FROM_HERE, callback_);
    }
  }

  void OnSingleTaskAborted() {
    animation_aborted_ = true;
    OnSingleTaskCompleted();
  }

  // The actual observer to listen each animation completion.
  class Observer : public ui::LayerAnimationObserver {
   public:
    Observer(ui::LayerAnimator* animator,
             CallbackRunningObserver* observer)
        : animator_(animator),
          observer_(observer) {}

   protected:
    // ui::LayerAnimationObserver overrides:
    virtual void OnLayerAnimationEnded(
        ui::LayerAnimationSequence* sequence) OVERRIDE {
      animator_->RemoveObserver(this);
      observer_->OnSingleTaskCompleted();
    }
    virtual void OnLayerAnimationAborted(
        ui::LayerAnimationSequence* sequence) OVERRIDE {
      animator_->RemoveObserver(this);
      observer_->OnSingleTaskAborted();
    }
    virtual void OnLayerAnimationScheduled(
        ui::LayerAnimationSequence* sequence) OVERRIDE {
    }
    virtual bool RequiresNotificationWhenAnimatorDestroyed() const OVERRIDE {
      return true;
    }

   private:
    ui::LayerAnimator* animator_;
    CallbackRunningObserver* observer_;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  size_t completed_counter_;
  bool animation_aborted_;
  ScopedVector<Observer> observer_list_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackRunningObserver);
};

}  // namespace

DisplayConfiguratorAnimation::DisplayConfiguratorAnimation()
    : weak_ptr_factory_(this) {
}

DisplayConfiguratorAnimation::~DisplayConfiguratorAnimation() {
  ClearHidingLayers();
}

void DisplayConfiguratorAnimation::StartFadeOutAnimation(
    base::Closure callback) {
  CallbackRunningObserver* observer = new CallbackRunningObserver(callback);
  ClearHidingLayers();

  // Make the fade-out animation for all root windows.  Instead of actually
  // hiding the root windows, we put a black layer over a root window for
  // safety.  These layers remain to hide root windows and will be deleted
  // after the animation of OnDisplayModeChanged().
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  for (aura::Window::Windows::const_iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    aura::Window* root_window = *it;
    ui::Layer* hiding_layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
    hiding_layer->SetColor(SK_ColorBLACK);
    hiding_layer->SetBounds(root_window->bounds());
    ui::Layer* parent =
        ash::Shell::GetContainer(root_window,
                                 ash::kShellWindowId_OverlayContainer)->layer();
    parent->Add(hiding_layer);

    hiding_layer->SetOpacity(0.0);

    ui::ScopedLayerAnimationSettings settings(hiding_layer->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kFadingAnimationDurationInMS));
    observer->AddNewAnimator(hiding_layer->GetAnimator());
    hiding_layer->SetOpacity(1.0f);
    hiding_layer->SetVisible(true);
    hiding_layers_[root_window] = hiding_layer;
  }

  // In case that OnDisplayModeChanged() isn't called or its animator is
  // canceled due to some unknown errors, we set a timer to clear these
  // hiding layers.
  timer_.reset(new base::OneShotTimer<DisplayConfiguratorAnimation>());
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kFadingTimeoutDurationInSeconds),
                this,
                &DisplayConfiguratorAnimation::ClearHidingLayers);
}

void DisplayConfiguratorAnimation::StartFadeInAnimation() {
  // We want to make sure clearing all of hiding layers after the animation
  // finished.  Note that this callback can be canceled, but the cancel only
  // happens when the next animation is scheduled.  Thus the hiding layers
  // should be deleted eventually.
  CallbackRunningObserver* observer = new CallbackRunningObserver(
      base::Bind(&DisplayConfiguratorAnimation::ClearHidingLayers,
                 weak_ptr_factory_.GetWeakPtr()));

  // Ensure that layers are not animating.
  for (std::map<aura::Window*, ui::Layer*>::iterator it =
           hiding_layers_.begin(); it != hiding_layers_.end(); ++it) {
    ui::LayerAnimator* animator = it->second->GetAnimator();
    if (animator->is_animating())
      animator->StopAnimating();
  }

  // Schedules the fade-in effect for all root windows.  Because we put the
  // black layers for fade-out, here we actually turn those black layers
  // invisible.
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  for (aura::Window::Windows::const_iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    aura::Window* root_window = *it;
    ui::Layer* hiding_layer = NULL;
    if (hiding_layers_.find(root_window) == hiding_layers_.end()) {
      // In case of the transition from mirroring->non-mirroring, new root
      // windows appear and we do not have the black layers for them.  Thus
      // we need to create the layer and make it visible.
      hiding_layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
      hiding_layer->SetColor(SK_ColorBLACK);
      hiding_layer->SetBounds(root_window->bounds());
      ui::Layer* parent =
          ash::Shell::GetContainer(
              root_window, ash::kShellWindowId_OverlayContainer)->layer();
      parent->Add(hiding_layer);
      hiding_layer->SetOpacity(1.0f);
      hiding_layer->SetVisible(true);
      hiding_layers_[root_window] = hiding_layer;
    } else {
      hiding_layer = hiding_layers_[root_window];
      if (hiding_layer->bounds() != root_window->bounds())
        hiding_layer->SetBounds(root_window->bounds());
    }

    ui::ScopedLayerAnimationSettings settings(hiding_layer->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kFadingAnimationDurationInMS));
    observer->AddNewAnimator(hiding_layer->GetAnimator());
    hiding_layer->SetOpacity(0.0f);
    hiding_layer->SetVisible(false);
  }
}

void DisplayConfiguratorAnimation::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& displays) {
  if (!hiding_layers_.empty())
    StartFadeInAnimation();
}

void DisplayConfiguratorAnimation::OnDisplayModeChangeFailed(
    ui::MultipleDisplayState failed_new_state) {
  if (!hiding_layers_.empty())
    StartFadeInAnimation();
}

void DisplayConfiguratorAnimation::ClearHidingLayers() {
  if (timer_) {
    timer_->Stop();
    timer_.reset();
  }
  STLDeleteContainerPairSecondPointers(
      hiding_layers_.begin(), hiding_layers_.end());
  hiding_layers_.clear();
}

}  // namespace ash
