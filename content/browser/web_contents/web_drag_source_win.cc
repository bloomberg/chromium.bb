// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_drag_source_win.h"

#include "base/bind.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_drag_utils_win.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"

using WebKit::WebDragOperationNone;

namespace content {
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
                             WebContents* web_contents)
    : ui::DragSourceWin(),
      source_wnd_(source_wnd),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      effect_(DROPEFFECT_NONE),
      data_(NULL) {
  registrar_.Add(this, NOTIFICATION_WEB_CONTENTS_SWAPPED,
                 Source<WebContents>(web_contents));
  registrar_.Add(this, NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                 Source<WebContents>(web_contents));
}

WebDragSource::~WebDragSource() {
}

void WebDragSource::OnDragSourceCancel() {
  // Delegate to the UI thread if we do drag-and-drop in the background thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebDragSource::OnDragSourceCancel, this));
    return;
  }

  if (!web_contents_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  web_contents_->DragSourceEndedAt(client.x(), client.y(),
                                   screen.x(), screen.y(),
                                   WebDragOperationNone);
}

void WebDragSource::OnDragSourceDrop() {
  DCHECK(data_);
  data_->SetInDragLoop(false);
  // On Windows, we check for drag end in IDropSource::QueryContinueDrag which
  // happens before IDropTarget::Drop is called. HTML5 requires the "dragend"
  // event to happen after the "drop" event. Since  Windows calls these two
  // directly after each other we can just post a task to handle the
  // OnDragSourceDrop after the current task.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebDragSource::DelayedOnDragSourceDrop, this));
}

void WebDragSource::DelayedOnDragSourceDrop() {
  if (!web_contents_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  web_contents_->DragSourceEndedAt(client.x(), client.y(), screen.x(),
                                   screen.y(), WinDragOpToWebDragOp(effect_));
}

void WebDragSource::OnDragSourceMove() {
  // Delegate to the UI thread if we do drag-and-drop in the background thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebDragSource::OnDragSourceMove, this));
    return;
  }

  if (!web_contents_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  web_contents_->DragSourceMovedTo(client.x(), client.y(),
                                   screen.x(), screen.y());
}

void WebDragSource::Observe(int type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  if (type == NOTIFICATION_WEB_CONTENTS_SWAPPED) {
    // When the WebContents get swapped, our render view host goes away.
    // That's OK, we can continue the drag, we just can't send messages back to
    // our drag source.
    web_contents_ = NULL;
  } else if (type == NOTIFICATION_WEB_CONTENTS_DISCONNECTED) {
    // This could be possible when we close the tab and the source is still
    // being used in DoDragDrop at the time that the virtual file is being
    // downloaded.
    web_contents_ = NULL;
  }
}

}  // namespace content
