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

  virtual bool HasKeyboardWindow() const OVERRIDE;
  virtual aura::Window* GetKeyboardWindow() OVERRIDE;

 private:
  // Overridden from keyboard::KeyboardControllerProxy:
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE;
  virtual ui::InputMethod* GetInputMethod() OVERRIDE;
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual void LoadSystemKeyboard() OVERRIDE;
  virtual void ReloadKeyboardIfNeeded() OVERRIDE;

  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<aura::Window> keyboard_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerProxyStub);
};

}  // namespace ash

#endif  // ASH_SHELL_KEYBOARD_CONTROLLER_PROXY_STUB_H_
