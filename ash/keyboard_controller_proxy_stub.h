// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_CONTROLLER_PROXY_STUB_H_
#define ASH_KEYBOARD_CONTROLLER_PROXY_STUB_H_

#include "ash/ash_export.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace ash {

// Stub implementation of KeyboardControllerProxy
class ASH_EXPORT KeyboardControllerProxyStub
    : public keyboard::KeyboardControllerProxy {
 public:
  KeyboardControllerProxyStub();
  virtual ~KeyboardControllerProxyStub();

 private:
  // Overridden from keyboard::KeyboardControllerProxy:
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE;
  virtual ui::InputMethod* GetInputMethod() OVERRIDE;
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerProxyStub);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_CONTROLLER_PROXY_STUB_H_
