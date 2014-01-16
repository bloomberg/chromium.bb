// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_keyboard_controller_proxy.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/keyboard/keyboard_controller.h"

namespace {

// Steps a layer animation until it is completed. Animations must be enabled.
void RunAnimationForLayer(ui::Layer* layer) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  gfx::AnimationContainerElement* element = layer->GetAnimator();
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    element->Step(step_time + base::TimeDelta::FromMilliseconds(1000));
  }
}

}  // namespace

class TestAshKeyboardControllerProxy : public AshKeyboardControllerProxy {
 public:
  TestAshKeyboardControllerProxy()
      : window_(new aura::Window(&delegate_)),
        input_method_(ui::CreateInputMethod(NULL,
                                            gfx::kNullAcceleratedWidget)) {
    window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    window_->set_owned_by_parent(false);
  }

  virtual ~TestAshKeyboardControllerProxy() {
    window_.reset();
  }

  // Overridden from AshKeyboardControllerProxy:
  virtual aura::Window* GetKeyboardWindow() OVERRIDE { return window_.get(); }
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE { return NULL; }
  virtual ui::InputMethod* GetInputMethod() OVERRIDE {
    return input_method_.get();
  }
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE {}

 private:
  scoped_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(TestAshKeyboardControllerProxy);
};

// TODO(bshe): Move this test back to unit test if
// ui::SetUpInputMethodFactoryForTesting() is safe to be called in unit test.
class AshKeyboardControllerProxyTest : public ash::test::AshTestBase {
 public:
  AshKeyboardControllerProxyTest() {}

  virtual ~AshKeyboardControllerProxyTest() {}

  // AshTestBase:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  TestAshKeyboardControllerProxy* proxy() { return proxy_; }
  keyboard::KeyboardController* controller() { return controller_.get(); }

 protected:
  void ShowKeyboard(aura::Window* container);
  void HideKeyboard(aura::Window* container);

  TestAshKeyboardControllerProxy* proxy_;
  scoped_ptr<keyboard::KeyboardController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerProxyTest);
};

void AshKeyboardControllerProxyTest::SetUp() {
  ui::SetUpInputMethodFactoryForTesting();
  AshTestBase::SetUp();
  proxy_ = new TestAshKeyboardControllerProxy();
  controller_.reset(new keyboard::KeyboardController(proxy_));

  aura::Window* keyboard_container(controller_->GetContainerWindow());
  keyboard_container->SetBounds(ash::Shell::GetPrimaryRootWindow()->bounds());
  ash::Shell::GetPrimaryRootWindow()->AddChild(keyboard_container);
  keyboard_container->AddChild(proxy_->GetKeyboardWindow());
  ASSERT_NE(proxy_->GetKeyboardWindow()->bounds().height(), 0);
}

void AshKeyboardControllerProxyTest::TearDown() {
  AshTestBase::TearDown();
}

void AshKeyboardControllerProxyTest::ShowKeyboard(aura::Window* container) {
  proxy_->ShowKeyboardContainer(container);
}

void AshKeyboardControllerProxyTest::HideKeyboard(aura::Window* container) {
  proxy_->HideKeyboardContainer(container);
}

TEST_F(AshKeyboardControllerProxyTest, VirtualKeyboardContainerAnimation) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  ui::Layer* layer = keyboard_container->layer();

  EXPECT_FALSE(keyboard_container->IsVisible());
  ShowKeyboard(keyboard_container);

  // Keyboard container and window should immediately become visible before
  // animation starts.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(proxy()->GetKeyboardWindow()->IsVisible());
  EXPECT_EQ(0.0, layer->opacity());
  gfx::Transform transform;
  transform.Translate(0, proxy()->GetKeyboardWindow()->bounds().height());
  EXPECT_EQ(transform, layer->transform());

  RunAnimationForLayer(layer);
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(proxy()->GetKeyboardWindow()->IsVisible());
  EXPECT_EQ(1.0, layer->opacity());
  EXPECT_EQ(gfx::Transform(), layer->transform());

  HideKeyboard(keyboard_container);
  // Keyboard container and window should be visible before hide animation
  // finishes.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(proxy()->GetKeyboardWindow()->IsVisible());

  RunAnimationForLayer(layer);
  EXPECT_FALSE(keyboard_container->IsVisible());
  EXPECT_FALSE(proxy()->GetKeyboardWindow()->IsVisible());
  EXPECT_EQ(0.0, layer->opacity());
  EXPECT_EQ(transform, layer->transform());
}
