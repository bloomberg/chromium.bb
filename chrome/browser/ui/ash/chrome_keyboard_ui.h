// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_UI_H_
#define CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_UI_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/keyboard/content/keyboard_ui_content.h"

namespace content {
class BrowserContext;
class WebContents;
}
namespace keyboard {
class KeyboardController;
class KeyboardControllerObserver;
}
namespace ui {
class InputMethod;
}

// Subclass of KeyboardUIContent. It is used by KeyboardController to get
// access to the virtual keyboard window and setup Chrome extension functions.
class ChromeKeyboardUI : public keyboard::KeyboardUIContent,
                         public content::WebContentsObserver {
 public:
  explicit ChromeKeyboardUI(content::BrowserContext* context);
  ~ChromeKeyboardUI() override;

  // keyboard::KeyboardUIContent overrides
  bool ShouldWindowOverscroll(aura::Window* window) const override;

 private:
  // keyboard::KeyboardUIContent overrides
  ui::InputMethod* GetInputMethod() override;
  void RequestAudioInput(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
  void SetupWebContents(content::WebContents* contents) override;
  void SetController(keyboard::KeyboardController* controller) override;
  void ShowKeyboardContainer(aura::Window* container) override;

  // The overridden implementation dispatches
  // chrome.virtualKeyboardPrivate.onTextInputBoxFocused event to extension to
  // provide the input type information. Naturally, when the virtual keyboard
  // extension is used as an IME then chrome.input.ime.onFocus provides the
  // information, but not when the virtual keyboard is used in conjunction with
  // another IME. virtualKeyboardPrivate.onTextInputBoxFocused is the remedy in
  // that case.
  void SetUpdateInputType(ui::TextInputType type) override;

  // content::WebContentsObserver overrides
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

  std::unique_ptr<keyboard::KeyboardControllerObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardUI);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_UI_H_
