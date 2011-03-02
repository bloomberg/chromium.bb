// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_view.h"

#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"

TabContentsView::TabContentsView(TabContents* tab_contents)
    : tab_contents_(tab_contents) {
}

TabContentsView::~TabContentsView() {}

void TabContentsView::RenderWidgetHostDestroyed(RenderWidgetHost* host) {
  if (host->view())
    host->view()->WillDestroyRenderWidget(host);
  delegate_view_helper_.RenderWidgetHostDestroyed(host);
}

void TabContentsView::RenderViewCreated(RenderViewHost* host) {
  // Default implementation does nothing. Platforms may override.
}

void TabContentsView::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  TabContents* new_contents = delegate_view_helper_.CreateNewWindow(
      route_id,
      tab_contents_->profile(),
      tab_contents_->GetSiteInstance(),
      WebUIFactory::GetWebUIType(tab_contents_->profile(),
          tab_contents_->GetURL()),
      tab_contents_,
      params.window_container_type,
      params.frame_name);

  if (new_contents) {
    NotificationService::current()->Notify(
        NotificationType::CREATING_NEW_WINDOW,
        Source<TabContents>(tab_contents_),
        Details<const ViewHostMsg_CreateWindow_Params>(&params));

    if (tab_contents_->delegate())
      tab_contents_->delegate()->TabContentsCreated(new_contents);
  }
}

void TabContentsView::CreateNewWidget(int route_id,
                                      WebKit::WebPopupType popup_type) {
  CreateNewWidgetInternal(route_id, popup_type);
}

void TabContentsView::CreateNewFullscreenWidget(int route_id) {
  CreateNewFullscreenWidgetInternal(route_id);
}

void TabContentsView::ShowCreatedWindow(int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture) {
  TabContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    tab_contents()->AddNewContents(contents, disposition, initial_pos,
                                   user_gesture);
  }
}

void TabContentsView::ShowCreatedWidget(int route_id,
                                        const gfx::Rect& initial_pos) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
  ShowCreatedWidgetInternal(widget_host_view, initial_pos);
}

void TabContentsView::Activate() {
  tab_contents_->Activate();
}

void TabContentsView::Deactivate() {
  tab_contents_->Deactivate();
}

void TabContentsView::ShowCreatedFullscreenWidget(int route_id) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
  ShowCreatedFullscreenWidgetInternal(widget_host_view);
}

void TabContentsView::LostCapture() {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->LostCapture();
}

bool TabContentsView::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  return tab_contents_->delegate() &&
    tab_contents_->delegate()->PreHandleKeyboardEvent(
        event, is_keyboard_shortcut);
}

void TabContentsView::UpdatePreferredSize(const gfx::Size& pref_size) {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->UpdatePreferredSize(pref_size);
}

bool TabContentsView::IsDoingDrag() const {
  return false;
}

bool TabContentsView::IsEventTracking() const {
  return false;
}

TabContentsView::TabContentsView() : tab_contents_(NULL) {}

void TabContentsView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->HandleKeyboardEvent(event);
}

void TabContentsView::HandleMouseUp() {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->HandleMouseUp();
}

void TabContentsView::HandleMouseActivate() {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->HandleMouseActivate();
}

RenderWidgetHostView* TabContentsView::CreateNewWidgetInternal(
    int route_id, WebKit::WebPopupType popup_type) {
  return delegate_view_helper_.CreateNewWidget(route_id, popup_type,
      tab_contents()->render_view_host()->process());
}

RenderWidgetHostView* TabContentsView::CreateNewFullscreenWidgetInternal(
    int route_id) {
  return delegate_view_helper_.CreateNewFullscreenWidget(
      route_id, tab_contents()->render_view_host()->process());
}

void TabContentsView::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view, const gfx::Rect& initial_pos) {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->RenderWidgetShowing();

  widget_host_view->InitAsPopup(tab_contents_->GetRenderWidgetHostView(),
                                initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
}

void TabContentsView::ShowCreatedFullscreenWidgetInternal(
    RenderWidgetHostView* widget_host_view) {
  if (tab_contents_->delegate())
    tab_contents_->delegate()->RenderWidgetShowing();

  widget_host_view->InitAsFullscreen();
  widget_host_view->GetRenderWidgetHost()->Init();
}
