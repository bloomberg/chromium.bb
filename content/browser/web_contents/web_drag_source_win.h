// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_SOURCE_WIN_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_SOURCE_WIN_H_

#include "base/basictypes.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/dragdrop/drag_source_win.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace content {
class RenderViewHost;
class WebContents;
class WebContentsImpl;

// An IDropSource implementation for a WebContentsImpl. Handles notifications
// sent by an active drag-drop operation as the user mouses over other drop
// targets on their system. This object tells Windows whether or not the drag
// should continue, and supplies the appropriate cursors.
class WebDragSource : public ui::DragSourceWin,
                      public NotificationObserver {
 public:
  // Create a new DragSource for a given HWND and WebContents.
  WebDragSource(gfx::NativeWindow source_wnd, WebContents* web_contents);
  virtual ~WebDragSource();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void set_effect(DWORD effect) { effect_ = effect; }
  // Used to set the active data object for the current drag operation. The
  // caller must ensure that |data| is not destroyed before the nested drag loop
  // terminates.
  void set_data(ui::OSExchangeData* data) { data_ = data; }

 protected:
  // ui::DragSourceWin
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
  WebContentsImpl* web_contents_;

  NotificationRegistrar registrar_;

  DWORD effect_;

  ui::OSExchangeData* data_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_SOURCE_WIN_H_
