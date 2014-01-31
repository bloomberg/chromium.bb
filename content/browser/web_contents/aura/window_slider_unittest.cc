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

void DispatchEventDuringScrollCallback(aura::WindowEventDispatcher* dispatcher,
                                       ui::Event* event,
                                       ui::EventType type,
                                       const gfx::Vector2dF& delta) {
  if (type != ui::ET_GESTURE_SCROLL_UPDATE)
    return;
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(event);
  CHECK(!details.dispatcher_destroyed);
}

void ChangeSliderOwnerDuringScrollCallback(scoped_ptr<aura::Window>* window,
                                           WindowSlider* slider,
                                           ui::EventType type,
                                           const gfx::Vector2dF& delta) {
  if (type != ui::ET_GESTURE_SCROLL_UPDATE)
    return;
  aura::Window* new_window = new aura::Window(NULL);
  new_window->Init(aura::WINDOW_LAYER_TEXTURED);
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
      : can_create_layer_(true),
        created_back_layer_(false),
        created_front_layer_(false),
        slide_completed_(false),
        slide_aborted_(false),
        slider_destroyed_(false) {
  }
  virtual ~WindowSliderDelegateTest() {}

  void Reset() {
    can_create_layer_ = true;
    created_back_layer_ = false;
    created_front_layer_ = false;
    slide_completed_ = false;
    slide_aborted_ = false;
    slider_destroyed_ = false;
  }

  void SetCanCreateLayer(bool can_create_layer) {
    can_create_layer_ = can_create_layer;
  }

  bool created_back_layer() const { return created_back_layer_; }
  bool created_front_layer() const { return created_front_layer_; }
  bool slide_completed() const { return slide_completed_; }
  bool slide_aborted() const { return slide_aborted_; }
  bool slider_destroyed() const { return slider_destroyed_; }

 protected:
  ui::Layer* CreateLayerForTest() {
    CHECK(can_create_layer_);
    ui::Layer* layer = new ui::Layer(ui::LAYER_SOLID_COLOR);
    layer->SetColor(SK_ColorRED);
    return layer;
  }

  // Overridden from WindowSlider::Delegate:
  virtual ui::Layer* CreateBackLayer() OVERRIDE {
    if (!can_create_layer_)
      return NULL;
    created_back_layer_ = true;
    return CreateLayerForTest();
  }

  virtual ui::Layer* CreateFrontLayer() OVERRIDE {
    if (!can_create_layer_)
      return NULL;
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

 private:
  bool can_create_layer_;
  bool created_back_layer_;
  bool created_front_layer_;
  bool slide_completed_;
  bool slide_aborted_;
  bool slider_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WindowSliderDelegateTest);
};

// This delegate destroys the owner window when the slider is destroyed.
class WindowSliderDeleteOwnerOnDestroy : public WindowSliderDelegateTest {
 public:
  explicit WindowSliderDeleteOwnerOnDestroy(aura::Window* owner)
      : owner_(owner) {
  }
  virtual ~WindowSliderDeleteOwnerOnDestroy() {}

 private:
  // Overridden from WindowSlider::Delegate:
  virtual void OnWindowSliderDestroyed() OVERRIDE {
    WindowSliderDelegateTest::OnWindowSliderDestroyed();
    delete owner_;
  }

  aura::Window* owner_;
  DISALLOW_COPY_AND_ASSIGN(WindowSliderDeleteOwnerOnDestroy);
};

// This delegate destroyes the owner window when a slide is completed.
class WindowSliderDeleteOwnerOnComplete : public WindowSliderDelegateTest {
 public:
  explicit WindowSliderDeleteOwnerOnComplete(aura::Window* owner)
      : owner_(owner) {
  }
  virtual ~WindowSliderDeleteOwnerOnComplete() {}

 private:
  // Overridden from WindowSlider::Delegate:
  virtual void OnWindowSlideComplete() OVERRIDE {
    WindowSliderDelegateTest::OnWindowSlideComplete();
    delete owner_;
  }

  aura::Window* owner_;
  DISALLOW_COPY_AND_ASSIGN(WindowSliderDeleteOwnerOnComplete);
};

typedef aura::test::AuraTestBase WindowSliderTest;

TEST_F(WindowSliderTest, WindowSlideUsingGesture) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;

  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  WindowSlider* slider =
      new WindowSlider(&slider_delegate, root_window(), window.get());
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(180, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();
  window->SetTransform(gfx::Transform());

  // Generat a horizontal overscroll in the reverse direction.
  generator.GestureScrollSequence(gfx::Point(180, 10),
                                  gfx::Point(10, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_front_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();

  // Generate a vertical overscroll.
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(10, 80),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();

  // Generate a horizontal scroll that starts overscroll, but doesn't scroll
  // enough to complete it.
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(80, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  EXPECT_FALSE(slider->IsSlideInProgress());
  slider_delegate.Reset();

  // Destroy the window. This should destroy the slider.
  window.reset();
  EXPECT_TRUE(slider_delegate.slider_destroyed());
}

// Tests that the window slide is cancelled when a different type of event
// happens.
TEST_F(WindowSliderTest, WindowSlideIsCancelledOnEvent) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  WindowSliderDelegateTest slider_delegate;

  ui::Event* events[] = {
    new ui::MouseEvent(ui::ET_MOUSE_MOVED,
                       gfx::Point(55, 10),
                       gfx::Point(55, 10),
                       0, 0),
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
                   root_window()->GetDispatcher(),
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
  scoped_ptr<WindowSlider> slider(
      new WindowSlider(&slider_delegate, parent.get(), window.get()));

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
                 slider.get()));
  aura::Window* new_window = window.get();
  EXPECT_NE(old_window, new_window);

  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
}

TEST_F(WindowSliderTest, NoSlideWhenLayerCantBeCreated) {
  scoped_ptr<aura::Window> window(CreateNormalWindow(0, root_window(), NULL));
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  WindowSliderDelegateTest slider_delegate;
  slider_delegate.SetCanCreateLayer(false);

  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  scoped_ptr<WindowSlider> slider(
      new WindowSlider(&slider_delegate, root_window(), window.get()));
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(160, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_FALSE(slider_delegate.created_back_layer());
  EXPECT_FALSE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
  window->SetTransform(gfx::Transform());

  slider_delegate.SetCanCreateLayer(true);
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(160, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());
}

// Tests that the owner window can be destroyed from |OnWindowSliderDestroyed()|
// delegate callback without causing a crash.
TEST_F(WindowSliderTest, OwnerIsDestroyedOnSliderDestroy) {
  size_t child_windows = root_window()->children().size();
  aura::Window* window = CreateNormalWindow(0, root_window(), NULL);
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  EXPECT_EQ(child_windows + 1, root_window()->children().size());

  WindowSliderDeleteOwnerOnDestroy slider_delegate(window);
  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  scoped_ptr<WindowSlider> slider(
      new WindowSlider(&slider_delegate, root_window(), window));
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(180, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_FALSE(slider_delegate.slider_destroyed());

  slider.reset();
  // Destroying the slider would have destroyed |window| too. So |window| should
  // not need to be destroyed here.
  EXPECT_EQ(child_windows, root_window()->children().size());
}

// Tests that the owner window can be destroyed from |OnWindowSlideComplete()|
// delegate callback without causing a crash.
TEST_F(WindowSliderTest, OwnerIsDestroyedOnSlideComplete) {
  size_t child_windows = root_window()->children().size();
  aura::Window* window = CreateNormalWindow(0, root_window(), NULL);
  window->SetBounds(gfx::Rect(0, 0, 400, 400));
  EXPECT_EQ(child_windows + 1, root_window()->children().size());

  WindowSliderDeleteOwnerOnComplete slider_delegate(window);
  aura::test::EventGenerator generator(root_window());

  // Generate a horizontal overscroll.
  new WindowSlider(&slider_delegate, root_window(), window);
  generator.GestureScrollSequence(gfx::Point(10, 10),
                                  gfx::Point(180, 10),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);
  EXPECT_TRUE(slider_delegate.created_back_layer());
  EXPECT_TRUE(slider_delegate.slide_completed());
  EXPECT_FALSE(slider_delegate.created_front_layer());
  EXPECT_FALSE(slider_delegate.slide_aborted());
  EXPECT_TRUE(slider_delegate.slider_destroyed());

  // Destroying the slider would have destroyed |window| too. So |window| should
  // not need to be destroyed here.
  EXPECT_EQ(child_windows, root_window()->children().size());
}

}  // namespace content
