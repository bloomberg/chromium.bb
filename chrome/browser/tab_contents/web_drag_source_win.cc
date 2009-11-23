// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_source_win.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"

using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;

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
    : BaseDragSource(),
      source_wnd_(source_wnd),
      render_view_host_(tab_contents->render_view_host()) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_SWAPPED,
                 Source<TabContents>(tab_contents));
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DISCONNECTED,
                 Source<TabContents>(tab_contents));
}

void WebDragSource::OnDragSourceCancel() {
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
  if (!render_view_host_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  render_view_host_->DragSourceEndedAt(client.x(), client.y(),
                                       screen.x(), screen.y(),
                                       WebDragOperationCopy);
  // TODO(jpa): This needs to be fixed to send the actual drag operation.
}

void WebDragSource::OnDragSourceMove() {
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
    NOTREACHED();
  }
}
