// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_
#define CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

class ExtensionFunctionDispatcher;

namespace content {
class WebContents;
}

namespace extensions {
class WindowController;
}

namespace ui {
class InputMethod;
}

class AshKeyboardControllerProxy
    : public keyboard::KeyboardControllerProxy,
      public content::WebContentsObserver,
      public ExtensionFunctionDispatcher::Delegate {
 public:
  AshKeyboardControllerProxy();
  virtual ~AshKeyboardControllerProxy();

  // keyboard::KeyboardControllerProxy overrides
  virtual aura::Window* GetKeyboardWindow() OVERRIDE;
  virtual ui::InputMethod* GetInputMethod() OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate overrides
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

 private:
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // content::WebContentsObserver overrides
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerProxy);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_
