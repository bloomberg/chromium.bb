// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

class RenderViewHost;
class TabContents;

// An IDropSource implementation for a TabContents. Handles notifications sent
// by an active drag-drop operation as the user mouses over other drop targets
// on their system. This object tells Windows whether or not the drag should
// continue, and supplies the appropriate cursors.
class WebDragSource : public ui::DragSource,
                      public NotificationObserver {
 public:
  // Create a new DragSource for a given HWND and TabContents.
  WebDragSource(gfx::NativeWindow source_wnd, TabContents* tab_contents);
  virtual ~WebDragSource();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void set_effect(DWORD effect) { effect_ = effect; }

 protected:
  // ui::DragSource
  virtual void OnDragSourceCancel();
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove();

 private:
  // Cannot construct thusly.
  WebDragSource();

  // OnDragSourceDrop schedules its main work to be done after IDropTarget::Drop
  // by posting a task to this function.
  void DelayedOnDragSourceDrop();

  // Keep a reference to the window so we can translate the cursor position.
  gfx::NativeWindow source_wnd_;

  // We use this as a channel to the renderer to tell it about various drag
  // drop events that it needs to know about (such as when a drag operation it
  // initiated terminates).
  RenderViewHost* render_view_host_;

  NotificationRegistrar registrar_;

  DWORD effect_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSource);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_SOURCE_WIN_H_
