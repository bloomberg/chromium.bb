// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_WINDOW_SLIDER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_WINDOW_SLIDER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace ui {
class Layer;
}

namespace content {

class ShadowLayerDelegate;

// A class for sliding the layer in a Window on top of other layers.
class CONTENT_EXPORT WindowSlider : public ui::EventHandler,
                                    public aura::WindowObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Creates a layer to show in the background, as the window-layer slides
    // with the scroll gesture.
    // The WindowSlider takes ownership of the created layer.
    virtual ui::Layer* CreateBackLayer() = 0;

    // Creates a layer to slide on top of the window-layer with the scroll
    // gesture.
    // The WindowSlider takes ownership of the created layer.
    virtual ui::Layer* CreateFrontLayer() = 0;

    // Called when the slide is complete. Note that at the end of a completed
    // slide, the window-layer may have been transformed. The callback here
    // should reset the transform if necessary.
    virtual void OnWindowSlideComplete() = 0;

    // Called when the slide is aborted. Note that when the slide is aborted,
    // the WindowSlider resets any transform it applied on the window-layer.
    virtual void OnWindowSlideAborted() = 0;

    // Called when the slider is destroyed.
    virtual void OnWindowSliderDestroyed() = 0;
  };

  // The WindowSlider slides the layers in the |owner| window. It starts
  // intercepting scroll events on |event_window|, and uses those events to
  // control the layer-slide. The lifetime of the slider is managed by the
  // lifetime of |owner|, i.e. if |owner| is destroyed, then the slider also
  // destroys itself.
  WindowSlider(Delegate* delegate,
               aura::Window* event_window,
               aura::Window* owner);

  virtual ~WindowSlider();

  // Changes the owner of the slider.
  void ChangeOwner(aura::Window* new_owner);

  bool IsSlideInProgress() const;

 private:
  // Sets up the slider layer correctly (sets the correct bounds of the layer,
  // parents it to the right layer, and sets up the correct stacking order).
  void SetupSliderLayer();

  void UpdateForScroll(float x_offset, float y_offset);

  void UpdateForFling(float x_velocity, float y_velocity);

  // Resets any in-progress slide.
  void ResetScroll();

  // Cancels any scroll/animation in progress.
  void CancelScroll();

  // The following callbacks are triggered after an animation.
  void CompleteWindowSlideAfterAnimation();

  void AbortWindowSlideAfterAnimation();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowRemovingFromRootWindow(aura::Window* window) OVERRIDE;

  Delegate* delegate_;

  // The slider intercepts scroll events from this window. The slider does not
  // own |event_window_|. If |event_window_| is destroyed, then the slider stops
  // listening for events, but it doesn't destroy itself.
  aura::Window* event_window_;

  // The window the slider operates on. The lifetime of the slider is bound to
  // this window (i.e. if |owner_| does, the slider destroys itself). The slider
  // can also delete itself when a slide gesture is completed. This does not
  // destroy |owner_|.
  aura::Window* owner_;

  // The accumulated amount of horizontal scroll.
  float delta_x_;

  // This keeps track of the layer created by the delegate.
  scoped_ptr<ui::Layer> slider_;

  // This manages the shadow for the layers.
  scoped_ptr<ShadowLayerDelegate> shadow_;

  base::WeakPtrFactory<WindowSlider> weak_factory_;

  float active_start_threshold_;

  const float start_threshold_touchscreen_;
  const float start_threshold_touchpad_;
  const float complete_threshold_;

  DISALLOW_COPY_AND_ASSIGN(WindowSlider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_WINDOW_SLIDER_H_
