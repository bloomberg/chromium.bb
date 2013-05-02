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
namespace gfx {
class Rect;
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

 private:
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // keyboard::KeyboardControllerProxy overrides
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE;
  virtual ui::InputMethod* GetInputMethod() OVERRIDE;
  virtual void SetupWebContents(content::WebContents* contents) OVERRIDE;
  virtual void OnKeyboardBoundsChanged(
      const gfx::Rect& keyboard_bounds) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate overrides
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // content::WebContentsObserver overrides
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerProxy);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_
