// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_KEYBOARD_KEYBOARD_MANAGER_H_
#define CHROME_BROWSER_UI_TOUCH_KEYBOARD_KEYBOARD_MANAGER_H_
#pragma once

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_observer.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#endif

namespace IPC {
class Message;
}

namespace ui {
class InterpolatedTransform;
class SlideAnimation;
}

namespace views {
class KeyEvent;
}

class DOMView;
class ExtensionHostMsg_Request_Params;

// A singleton object to manage the virtual keyboard.
class KeyboardManager
    : public views::Widget,
      public ui::AnimationDelegate,
      public BrowserList::Observer,
      public TabContentsObserver,
      public TabStripModelObserver,
      public ExtensionFunctionDispatcher::Delegate,
#if defined(OS_CHROMEOS)
      public chromeos::input_method::InputMethodManager::VirtualKeyboardObserver,
#endif
      public NotificationObserver {
 public:
  // Returns the singleton object.
  static KeyboardManager* GetInstance();

  // Show the keyboard for the target widget. The events from the keyboard will
  // be sent to |widget|.
  // TODO(sad): Allow specifying the type of keyboard to show.
  void ShowKeyboardForWidget(views::Widget* widget);

  // Overridden from views::Widget
  void Hide() OVERRIDE;

 private:
  // Requirement for Singleton
  friend struct DefaultSingletonTraits<KeyboardManager>;

  KeyboardManager();
  virtual ~KeyboardManager();

  // Overridden from views::Widget.
  virtual bool OnKeyEvent(const views::KeyEvent& event) OVERRIDE;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Overridden from BrowserList::Observer.
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE;

  // Overridden from TabContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // Overridden from TabStripModelObserver.
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture);

  // ExtensionFunctionDispatcher::Delegate implementation
  virtual Browser* GetBrowser() OVERRIDE;
  virtual gfx::NativeView GetNativeViewOfHost() OVERRIDE;
  virtual TabContents* GetAssociatedTabContents() const OVERRIDE;

#if defined(OS_CHROMEOS)
  // input_method::InputMethodManager::VirtualKeyboardObserver implementation.
  virtual void VirtualKeyboardChanged(
      chromeos::input_method::InputMethodManager* manager,
      const chromeos::input_method::VirtualKeyboard& virtual_keyboard,
      const std::string& virtual_keyboard_layout);
#endif

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // The animation.
  scoped_ptr<ui::SlideAnimation> animation_;

  // Interpolated transform used during animation.
  scoped_ptr<ui::InterpolatedTransform> transform_;

  // The DOM view to host the keyboard.
  DOMView* dom_view_;

  ExtensionFunctionDispatcher extension_dispatcher_;

  // The widget the events from the keyboard should be directed to.
  views::Widget* target_;

  // Height of the keyboard.
  int keyboard_height_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardManager);
};

#endif  // CHROME_BROWSER_UI_TOUCH_KEYBOARD_KEYBOARD_MANAGER_H_
