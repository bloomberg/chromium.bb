// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DROP_TARGET_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DROP_TARGET_WIN_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/base/dragdrop/drop_target.h"

class InterstitialDropTarget;
class RenderViewHost;
class TabContents;

// A helper object that provides drop capabilities to a TabContents. The
// DropTarget handles drags that enter the region of the TabContents by
// passing on the events to the renderer.
class WebDropTarget : public ui::DropTarget {
 public:
  // Create a new WebDropTarget associating it with the given HWND and
  // TabContents.
  WebDropTarget(HWND source_hwnd, TabContents* contents);
  virtual ~WebDropTarget();

  void set_drag_cursor(WebKit::WebDragOperation op) {
    drag_cursor_ = op;
  }

 protected:
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);

  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);

  virtual void OnDragLeave(IDataObject* data_object);

  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // Our associated TabContents.
  TabContents* tab_contents_;

  // We keep track of the render view host we're dragging over.  If it changes
  // during a drag, we need to re-send the DragEnter message.  WARNING:
  // this pointer should never be dereferenced.  We only use it for comparing
  // pointers.
  RenderViewHost* current_rvh_;

  // Used to determine what cursor we should display when dragging over web
  // content area.  This can be updated async during a drag operation.
  WebKit::WebDragOperation drag_cursor_;

  // A special drop target handler for when we try to d&d while an interstitial
  // page is showing.
  scoped_ptr<InterstitialDropTarget> interstitial_drop_target_;

  DISALLOW_COPY_AND_ASSIGN(WebDropTarget);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_DROP_TARGET_WIN_H_
