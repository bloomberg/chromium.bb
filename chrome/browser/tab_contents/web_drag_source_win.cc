// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_source_win.h"

#include "base/task.h"
#include "chrome/browser/tab_contents/web_drag_utils_win.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

using WebKit::WebDragOperationNone;

namespace {

static void GetCursorPositions(gfx::NativeWindow wnd, gfx::Point* client,
                               gfx::Point* screen) {
  POINT cursor_pos;
  GetCursorPos(&cursor_pos);
  screen->SetPoint(cursor_pos.x, cursor_pos.y);
  ScreenToClient(wnd, &cursor_pos);
  client->SetPoint(cursor_pos.x, cursor_pos.y);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WebDragSource, public:

WebDragSource::WebDragSource(gfx::NativeWindow source_wnd,
                             TabContents* tab_contents)
    : ui::DragSource(),
      source_wnd_(source_wnd),
      render_view_host_(tab_contents->render_view_host()),
      effect_(DROPEFFECT_NONE) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_SWAPPED,
                 Source<TabContents>(tab_contents));
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DISCONNECTED,
                 Source<TabContents>(tab_contents));
}

WebDragSource::~WebDragSource() {
}

void WebDragSource::OnDragSourceCancel() {
  // Delegate to the UI thread if we do drag-and-drop in the background thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &WebDragSource::OnDragSourceCancel));
    return;
  }

  if (!render_view_host_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  render_view_host_->DragSourceEndedAt(client.x(), client.y(),
                                       screen.x(), screen.y(),
                                       WebDragOperationNone);
}

void WebDragSource::OnDragSourceDrop() {
  // On Windows, we check for drag end in IDropSource::QueryContinueDrag which
  // happens before IDropTarget::Drop is called. HTML5 requires the "dragend"
  // event to happen after the "drop" event. Since  Windows calls these two
  // directly after each other we can just post a task to handle the
  // OnDragSourceDrop after the current task.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &WebDragSource::DelayedOnDragSourceDrop));
}

void WebDragSource::DelayedOnDragSourceDrop() {
  if (!render_view_host_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  render_view_host_->DragSourceEndedAt(
      client.x(), client.y(), screen.x(), screen.y(),
      web_drag_utils_win::WinDragOpToWebDragOp(effect_));
}

void WebDragSource::OnDragSourceMove() {
  // Delegate to the UI thread if we do drag-and-drop in the background thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &WebDragSource::OnDragSourceMove));
    return;
  }

  if (!render_view_host_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  render_view_host_->DragSourceMovedTo(client.x(), client.y(),
                                       screen.x(), screen.y());
}

void WebDragSource::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  if (NotificationType::TAB_CONTENTS_SWAPPED == type) {
    // When the tab contents get swapped, our render view host goes away.
    // That's OK, we can continue the drag, we just can't send messages back to
    // our drag source.
    render_view_host_ = NULL;
  } else if (NotificationType::TAB_CONTENTS_DISCONNECTED == type) {
    // This could be possible when we close the tab and the source is still
    // being used in DoDragDrop at the time that the virtual file is being
    // downloaded.
    render_view_host_ = NULL;
  }
}
