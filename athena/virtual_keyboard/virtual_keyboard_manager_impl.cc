// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/virtual_keyboard/public/virtual_keyboard_manager.h"

#include "athena/common/container_priorities.h"
#include "athena/common/fill_layout_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_util.h"

namespace athena {

namespace {

VirtualKeyboardManager* instance;

// A very basic and simple implementation of KeyboardControllerProxy.
class BasicKeyboardControllerProxy : public keyboard::KeyboardControllerProxy {
 public:
  BasicKeyboardControllerProxy(content::BrowserContext* context,
                               aura::Window* root_window)
      : browser_context_(context), root_window_(root_window) {}
  virtual ~BasicKeyboardControllerProxy() {}

  // keyboard::KeyboardControllerProxy:
  virtual ui::InputMethod* GetInputMethod() OVERRIDE {
    ui::InputMethod* input_method =
        root_window_->GetProperty(aura::client::kRootWindowInputMethodKey);
    return input_method;
  }

  virtual void RequestAudioInput(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE {}

  virtual content::BrowserContext* GetBrowserContext() OVERRIDE {
    return browser_context_;
  }

  virtual void SetUpdateInputType(ui::TextInputType type) OVERRIDE {}

 private:
  content::BrowserContext* browser_context_;
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(BasicKeyboardControllerProxy);
};

class VirtualKeyboardManagerImpl : public VirtualKeyboardManager {
 public:
  explicit VirtualKeyboardManagerImpl(content::BrowserContext* browser_context)
      : browser_context_(browser_context),
        container_(NULL) {
    CHECK(!instance);
    instance = this;
    Init();
  }

  virtual ~VirtualKeyboardManagerImpl() {
    CHECK_EQ(this, instance);
    instance = NULL;

    keyboard::KeyboardController::ResetInstance(NULL);
  }

 private:
  void Init() {
    athena::ScreenManager::ContainerParams params("VirtualKeyboardContainer",
                                                  CP_VIRTUAL_KEYBOARD);
    container_ = athena::ScreenManager::Get()->CreateContainer(params);
    container_->SetLayoutManager(new FillLayoutManager(container_));

    keyboard_controller_.reset(new keyboard::KeyboardController(
        new BasicKeyboardControllerProxy(browser_context_,
                                         container_->GetRootWindow())));
    keyboard::KeyboardController::ResetInstance(keyboard_controller_.get());
    aura::Window* kb_container = keyboard_controller_->GetContainerWindow();
    container_->AddChild(kb_container);
    kb_container->Show();
  }

  content::BrowserContext* browser_context_;
  aura::Window* container_;
  scoped_ptr<keyboard::KeyboardController> keyboard_controller_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardManagerImpl);
};

}  // namespace

// static
VirtualKeyboardManager* VirtualKeyboardManager::Create(
    content::BrowserContext* browser_context) {
  CHECK(!instance);
  keyboard::InitializeKeyboard();
  keyboard::SetTouchKeyboardEnabled(true);
  keyboard::InitializeWebUIBindings();

  new VirtualKeyboardManagerImpl(browser_context);
  CHECK(instance);
  return instance;
}

VirtualKeyboardManager* VirtualKeyboardManager::Get() {
  return instance;
}

void VirtualKeyboardManager::Shutdown() {
  CHECK(instance);
  delete instance;
  CHECK(!instance);
}

}  // namespace athena
