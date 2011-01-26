// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class BrowserFrame;
class BrowserView;
class KeyboardContainerView;
class NotificationDetails;
class NotificationSource;

class TouchBrowserFrameView : public OpaqueBrowserFrameView,
                              public TabStripModelObserver,
                              public NotificationObserver {
 public:
  // Constructs a non-client view for an BrowserFrame.
  TouchBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~TouchBrowserFrameView();

  // Overridden from OpaqueBrowserFrameView
  virtual void Layout();

 protected:
  // Overridden from OpaqueBrowserFrameView
  virtual int GetReservedHeight() const;

 private:
  virtual void InitVirtualKeyboard();
  virtual void UpdateKeyboardAndLayout(bool should_show_keyboard);

  // Overrridden from TabStripModelObserver.
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture);

  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  bool keyboard_showing_;
  KeyboardContainerView* keyboard_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TouchBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
