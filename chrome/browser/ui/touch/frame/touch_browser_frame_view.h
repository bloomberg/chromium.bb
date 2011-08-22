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

class TouchBrowserFrameView
    : public OpaqueBrowserFrameView,
      public views::FocusChangeListener {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // Constructs a non-client view for an BrowserFrame.
  TouchBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~TouchBrowserFrameView();

 private:
  // views::FocusChangeListener implementation
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

  // Overridden from views::View
  virtual std::string GetClassName() const OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual bool HitTest(const gfx::Point& point) const OVERRIDE;

  bool focus_listener_added_;

  DISALLOW_COPY_AND_ASSIGN(TouchBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
