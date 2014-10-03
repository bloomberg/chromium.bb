// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_KEYBOARD_CONTROLLER_PROXY_STUB_H_
#define ASH_SHELL_KEYBOARD_CONTROLLER_PROXY_STUB_H_

#include "ui/aura/test/test_window_delegate.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Stub implementation of KeyboardControllerProxy
class KeyboardControllerProxyStub : public keyboard::KeyboardControllerProxy {
 public:
  KeyboardControllerProxyStub();
  virtual ~KeyboardControllerProxyStub();

  virtual bool HasKeyboardWindow() const override;
  virtual aura::Window* GetKeyboardWindow() override;

 private:
  // Overridden from keyboard::KeyboardControllerProxy:
  virtual content::BrowserContext* GetBrowserContext() override;
  virtual ui::InputMethod* GetInputMethod() override;
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
  virtual void LoadSystemKeyboard() override;
  virtual void ReloadKeyboardIfNeeded() override;

  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<aura::Window> keyboard_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerProxyStub);
};

}  // namespace ash

#endif  // ASH_SHELL_KEYBOARD_CONTROLLER_PROXY_STUB_H_
