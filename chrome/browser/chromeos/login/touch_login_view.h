// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TOUCH_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TOUCH_LOGIN_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/focus/focus_manager.h"

class KeyboardContainerView;
class NotificationDetails;
class NotificationSource;

namespace ui {
class SlideAnimation;
}

namespace chromeos {

// Subclass of the WebUILoginView. This view adds in support for a virtual
// keyboard, which appears and disappears depending if text areas have
// focus. This is only build in TOUCH_UI enabled builds.
class TouchLoginView : public WebUILoginView,
                       public views::FocusChangeListener,
                       public NotificationObserver,
                       public ui::AnimationDelegate {
 public:
  enum VirtualKeyboardType {
    NONE,
    GENERIC,
    URL,
  };

  TouchLoginView();
  virtual ~TouchLoginView();

  // Overriden from WebUILoginView:
  virtual void Init() OVERRIDE;

  // Overriden from views::Views:
  virtual std::string GetClassName() const OVERRIDE;

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now) OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

 private:
  void InitVirtualKeyboard();
  void UpdateKeyboardAndLayout(bool should_show_keyboard);
  VirtualKeyboardType DecideKeyboardStateForView(views::View* view);

  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  bool keyboard_showing_;
  int keyboard_height_;
  bool focus_listener_added_;
  KeyboardContainerView* keyboard_;
  NotificationRegistrar registrar_;

  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(TouchLoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TOUCH_LOGIN_VIEW_H_
