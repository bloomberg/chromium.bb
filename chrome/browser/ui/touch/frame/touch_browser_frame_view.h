// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/focus/focus_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#endif

class BrowserFrame;
class BrowserView;
class KeyboardContainerView;
class NotificationDetails;
class NotificationSource;

namespace ui {
class SlideAnimation;
}

class TouchBrowserFrameView
    : public OpaqueBrowserFrameView,
      public views::FocusChangeListener,
      public TabStripModelObserver,
#if defined(OS_CHROMEOS)
      public
      chromeos::input_method::InputMethodManager::VirtualKeyboardObserver,
#endif
      public ui::AnimationDelegate {
 public:
  enum VirtualKeyboardType {
    NONE,
    GENERIC,
    URL,
  };

  // Internal class name.
  static const char kViewClassName[];

  // Constructs a non-client view for an BrowserFrame.
  TouchBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~TouchBrowserFrameView();

  // Overriden from Views.
  virtual std::string GetClassName() const OVERRIDE;

  // Overridden from OpaqueBrowserFrameView
  virtual void Layout();

  // views::FocusChangeListener implementation
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

#if defined(OS_CHROMEOS)
  // input_method::InputMethodManager::VirtualKeyboardObserver implementation.
  virtual void VirtualKeyboardChanged(
      chromeos::input_method::InputMethodManager* manager,
      const chromeos::input_method::VirtualKeyboard& virtual_keyboard,
      const std::string& virtual_keyboard_layout);
#endif

 protected:
  // Overridden from OpaqueBrowserFrameView
  virtual int GetReservedHeight() const;
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

 private:
  virtual void InitVirtualKeyboard();
  virtual void UpdateKeyboardAndLayout(bool should_show_keyboard);
  virtual VirtualKeyboardType DecideKeyboardStateForView(views::View* view);

  // Overridden from views::View
  virtual bool HitTest(const gfx::Point& point) const OVERRIDE;

  // Overrridden from TabStripModelObserver.
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture);
  virtual void TabStripEmpty();

  // Overridden from NotificationObserver.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation);
  virtual void AnimationEnded(const ui::Animation* animation);

  bool keyboard_showing_;
  int keyboard_height_;
  bool focus_listener_added_;
  KeyboardContainerView* keyboard_;
  NotificationRegistrar registrar_;
  GURL url_;

  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(TouchBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
