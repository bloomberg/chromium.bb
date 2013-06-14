// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/window_slider.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace content {

void DispatchEventDuringScrollCallback(aura::RootWindow* root_window,
                                       ui::Event* event,
                                       ui::EventType type,
                                       const gfx::Vector2dF& delta) {
  if (type != ui::ET_GESTURE_SCROLL_UPDATE)
    return;
  aura::RootWindowHostDelegate* delegate =
      root_window->AsRootWindowHostDelegate();
  if (event->IsMouseEvent())
    delegate->OnHostMouseEvent(static_cast<ui::MouseEvent*>(event));
  else if (event->IsKeyEvent())
    delegate->OnHostKeyEvent(static_cast<ui::KeyEvent*>(event));
}

void ChangeSliderOwnerDuringScrollCallback(scoped_ptr<aura::Window>* window,
                                           WindowSlider* slider,
                                           ui::EventType type,
                                           const gfx::Vector2dF& delta) {
  if (type != ui::ET_GESTURE_SCROLL_UPDATE)
    return;
  aura::Window* new_window = new aura::Window(NULL);
  new_window->Init(ui::LAYER_TEXTURED);
  new_window->Show();
  slider->ChangeOwner(new_window);
  (*window)->parent()->AddChild(new_window);
  window->reset(new_window);
}

// The window delegate does not receive any events.
class NoEventWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  NoEventWindowDelegate() {
  }
  virtual ~NoEventWindowDelegate() {}

 private:
  // Overridden from aura::WindowDelegate:
  virtual bool HasHitTestMask() const OVERRIDE { return true; }

  DISALLOW_COPY_AND_ASSIGN(NoEventWindowDelegate);
};

class WindowSliderDelegateTest : public WindowSlider::Delegate {
 public:
  WindowSliderDelegateTest()
      : created_back_layer_(false),
        created_front_layer_(false),
        slide_completed_(false),
        slide_aborted_(false),
        slider_destroyed_(false) {
  }
  virtual ~WindowSliderDelegateTest() {}

  void Reset() {
    created_back_layer_ = false;
    created_front_layer_ = false;
    slide_completed_ = false;
    slide_aborted_ = false;
    slider_destroyed_ = false;
  }

  bool created_back_layer() const { return created_back_layer_; }
  bool created_front_layer() const { return created_front_layer_; }
  bool slide_completed() const { return slide_completed_; }
  bool slide_aborted() const { return slide_aborted_; }
  bool slider_destroyed() const { return slider_destroyed_; }

 private:
  ui::Layer* CreateLayerForTest() {
    ui::Layer* layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
    layer->SetColor(SK_ColorRED);
    return layer;
  }

  // Overridden from WindowSlider::Delegate:
  virtual ui::Layer* CreateBackLayer() OVERRIDE {
    created_back_layer_ = true;
    return CreateLayerForTest();
  }

  virtual ui::Layer* CreateFrontLayer() OVERRIDE {
    created_front_layer_ = true;
    return CreateLayerForTest();
  }

  virtual void OnWindowSlideComplete() OVERRIDE {
    slide_completed_ = true;
  }

  virtual void OnWindowSlideAborted() OVERRIDE {
    slide_aborted_ = true;
  }

  virtual void OnWindowSliderDestroyed() OVERRIDE {
    slider_destroyed_ = true;
  }

  bool created_back_layer_;
  bool created_front_layer_;
  bool slide_completed_;
  bool slide_aborted_;
  bool slider_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WindowSliderDelegateTest);
};

typedef aura::test::AuraTestBase WindowSliderTest;

#if defined(OS_WIN)
// This test currently fails on the Win Aura (2) bot. http://crbug.com/249634
#define MAYBE_WindowSlideUsingGesture DISABLED_WindowSlideUsingGesture
#else
#define MAYBE_WindowSlideUsingGesture WindowSlideUsingGesture
#endif

TEST_F(WindowSliderTest, MAYBE_WindowSlideUsingGesture) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;

  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  new WindowSlider(&slider_delegate, root_window(), window.get());
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(160, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_TRUE(slider_delegate.slider_destroyed());
  slider_delegate.Reset();
  window->SetTransform(gfx::Transform());

  // Since the slider has been destroyed, recreate a new one.
  new WindowSlider(&slider_delegate, root_window(), window.get());

  // Generat a horizontal overscroll in the reverse direction.
  generator.GestureScrollSequence(gfx::Point(160, 10),
                                  gfx::Point(10, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_front_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_TRUE(slider_delegate.slider_destroyed());
  slider_delegate.Reset();

  // Since the slider has been destroyed, recreate a new one.
  new WindowSlider(&slider_delegate, root_window(), window.get());

  // Generate a vertical overscroll.
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(10, 80),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  slider_delegate.Reset();

  // Generate a horizontal scroll that starts overscroll, but doesn't scroll
  // enough to complete it.
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(60, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  slider_delegate.Reset();

  // Destroy the window. This should destroy the slider.
  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

#if defined(OS_WIN)
// This test currently fails on the Win Aura (2) bot. http://crbug.com/249634
#define MAYBE_WindowSlideIsCancelledOnEvent \
    DISABLED_WindowSlideIsCancelledOnEvent
#else
#define MAYBE_WindowSlideIsCancelledOnEvent WindowSlideIsCancelledOnEvent
#endif

// Tests that the window slide is cancelled when a different type of event
// happens.
TEST_F(WindowSliderTest, MAYBE_WindowSlideIsCancelledOnEvent) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  WindowSliderDelegateTest slider_delegate;

  ui::Event* events[] = {
    new ui::MouseEvent(ui::ET_MOUSE_MOVED,
                       gfx::Point(55, 10),
                       gfx::Point(55, 10),
                       0),
    new ui::KeyEvent(ui::ET_KEY_PRESSED,
                     ui::VKEY_A,
                     0,
                     true),
    NULL
  };

  new WindowSlider(&slider_delegate, root_window(), window.get());
  for (int i = 0; events[i]; ++i) {
    // Generate a horizontal overscroll.
    aura::test::EventGenerator generator(root_window());
    generator.GestureScrollSequenceWithCallback(
        gfx::Point(10, 10),
        gfx::Point(80, 10),
        base::TimeDelta::FromMilliseconds(10),
        1,
        base::Bind(&DispatchEventDuringScrollCallback,
                   root_window(),
                   base::Owned(events[i])));
    EXPECT_TRUE(slider_delegate.created_back_layer());
    EXPECT_TRUE(slider_delegate.slide_aborted());
    EXPECT_FALSE(slider_delegate.created_front_layer());
    EXPECT_FALSE(slider_delegate.slide_completed());
    EXPECT_FALSE(slider_delegate.slider_destroyed());
    slider_delegate.Reset();
  }
  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

// Tests that the slide works correctly when the owner of the window changes
// during the duration of the slide.
TEST_F(WindowSliderTest, OwnerWindowChangesDuringWindowSlide) {
  scoped_ptr<aura::Window> parent(CreateNormalWindow(0, root_window(), NULL));

  NoEventWindowDelegate window_delegate;
  window_delegate.set_window_component(HTNOWHERE);
  scoped_ptr<aura::Window> window(CreateNormalWindow(1, parent.get(),
                                                     &window_delegate));

  WindowSliderDelegateTest slider_delegate;
  WindowSlider* slider = new WindowSlider(&slider_delegate, parent.get(),
                                          window.get());

  // Generate a horizontal scroll, and change the owner in the middle of the
  // scroll.
  aura::test::EventGenerator generator(root_window());
  aura::Window* old_window = window.get();
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(10, 10),
      gfx::Point(80, 10),
      base::TimeDelta::FromMilliseconds(10),
      1,
      base::Bind(&ChangeSliderOwnerDuringScrollCallback,
                 base::Unretained(&window),
                 slider));
  aura::Window* new_window = window.get();
  EXPECT_NE(old_window, new_window);

  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

}  // namespace content
