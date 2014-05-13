// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_
#define CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace content {
class WebContents;
}
namespace extensions {
class ExtensionFunctionDispatcher;
class WindowController;
}
namespace gfx {
class Rect;
}
namespace ui {
class InputMethod;
}

// Subclass of KeyboardControllerProxy. It is used by KeyboardController to get
// access to the virtual keyboard window and setup Chrome extension functions.
class AshKeyboardControllerProxy
    : public keyboard::KeyboardControllerProxy,
      public content::WebContentsObserver,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  AshKeyboardControllerProxy();
  virtual ~AshKeyboardControllerProxy();

 private:
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // keyboard::KeyboardControllerProxy overrides
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE;
  virtual ui::InputMethod* GetInputMethod() OVERRIDE;
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual void SetupWebContents(content::WebContents* contents) OVERRIDE;
  virtual void ShowKeyboardContainer(aura::Window* container) OVERRIDE;

  // The overridden implementation dispatches
  // chrome.virtualKeyboardPrivate.onTextInputBoxFocused event to extension to
  // provide the input type information. Naturally, when the virtual keyboard
  // extension is used as an IME then chrome.input.ime.onFocus provides the
  // information, but not when the virtual keyboard is used in conjunction with
  // another IME. virtualKeyboardPrivate.onTextInputBoxFocused is the remedy in
  // that case.
  virtual void SetUpdateInputType(ui::TextInputType type) OVERRIDE;

  // extensions::ExtensionFunctionDispatcher::Delegate overrides
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // content::WebContentsObserver overrides
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  scoped_ptr<extensions::ExtensionFunctionDispatcher>
      extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerProxy);
};

#endif  // CHROME_BROWSER_UI_ASH_ASH_KEYBOARD_CONTROLLER_PROXY_H_
