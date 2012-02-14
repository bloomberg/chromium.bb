// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_view_helper.h"

#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"

using content::WebContents;

TabContentsViewHelper::TabContentsViewHelper() {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

TabContentsViewHelper::~TabContentsViewHelper() {}

void TabContentsViewHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED);
  RenderWidgetHost* host = content::Source<RenderWidgetHost>(source).ptr();
  for (PendingWidgetViews::iterator i = pending_widget_views_.begin();
       i != pending_widget_views_.end(); ++i) {
    if (host->view() == i->second) {
      pending_widget_views_.erase(i);
      break;
    }
  }
}

TabContents* TabContentsViewHelper::CreateNewWindow(
    WebContents* web_contents,
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  bool should_create = true;
  if (web_contents->GetDelegate()) {
    should_create = web_contents->GetDelegate()->ShouldCreateWebContents(
        web_contents,
        route_id,
        params.window_container_type,
        params.frame_name);
  }

  if (!should_create)
    return NULL;

  // Create the new web contents. This will automatically create the new
  // WebContentsView. In the future, we may want to create the view separately.
  TabContents* new_contents =
      new TabContents(web_contents->GetBrowserContext(),
                      web_contents->GetSiteInstance(),
                      route_id,
                      static_cast<TabContents*>(web_contents),
                      NULL);
  new_contents->set_opener_web_ui_type(
      web_contents->GetWebUITypeForCurrentState());
  content::WebContentsView* new_view = new_contents->GetView();

  // TODO(brettw): It seems bogus that we have to call this function on the
  // newly created object and give it one of its own member variables.
  new_view->CreateViewForWidget(new_contents->GetRenderViewHost());

  // Save the created window associated with the route so we can show it later.
  pending_contents_[route_id] = new_contents;

  if (web_contents->GetDelegate())
    web_contents->GetDelegate()->WebContentsCreated(web_contents,
                                                    params.opener_frame_id,
                                                    params.target_url,
                                                    new_contents);

  return new_contents;
}

RenderWidgetHostView* TabContentsViewHelper::CreateNewWidget(
    WebContents* web_contents,
    int route_id,
    bool is_fullscreen,
    WebKit::WebPopupType popup_type) {
  content::RenderProcessHost* process = web_contents->GetRenderProcessHost();
  RenderWidgetHost* widget_host = new RenderWidgetHost(process, route_id);
  RenderWidgetHostView* widget_view =
      RenderWidgetHostView::CreateViewForWidget(widget_host);
  if (!is_fullscreen) {
    // Popups should not get activated.
    widget_view->set_popup_type(popup_type);
  }
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[route_id] = widget_view;
  return widget_view;
}

TabContents* TabContentsViewHelper::GetCreatedWindow(int route_id) {
  PendingContents::iterator iter = pending_contents_.find(route_id);

  // Certain systems can block the creation of new windows. If we didn't succeed
  // in creating one, just return NULL.
  if (iter == pending_contents_.end()) {
    return NULL;
  }

  TabContents* new_contents = iter->second;
  pending_contents_.erase(route_id);

  if (!new_contents->GetRenderProcessHost()->HasConnection() ||
      !new_contents->GetRenderViewHost()->view())
    return NULL;

  // TODO(brettw): It seems bogus to reach into here and initialize the host.
  new_contents->GetRenderViewHost()->Init();
  return new_contents;
}

RenderWidgetHostView* TabContentsViewHelper::GetCreatedWidget(int route_id) {
  PendingWidgetViews::iterator iter = pending_widget_views_.find(route_id);
  if (iter == pending_widget_views_.end()) {
    DCHECK(false);
    return NULL;
  }

  RenderWidgetHostView* widget_host_view = iter->second;
  pending_widget_views_.erase(route_id);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->process()->HasConnection()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return NULL;
  }

  return widget_host_view;
}

TabContents* TabContentsViewHelper::ShowCreatedWindow(
    WebContents* web_contents,
    int route_id,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  TabContents* contents = GetCreatedWindow(route_id);
  if (contents) {
    web_contents->AddNewContents(contents,
                                 disposition,
                                 initial_pos,
                                 user_gesture);
  }
  return contents;
}

RenderWidgetHostView* TabContentsViewHelper::ShowCreatedWidget(
    WebContents* web_contents,
    int route_id,
    bool is_fullscreen,
    const gfx::Rect& initial_pos) {
  if (web_contents->GetDelegate())
    web_contents->GetDelegate()->RenderWidgetShowing();

  RenderWidgetHostView* widget_host_view = GetCreatedWidget(route_id);
  if (is_fullscreen) {
    widget_host_view->InitAsFullscreen(web_contents->GetRenderWidgetHostView());
  } else {
    widget_host_view->InitAsPopup(web_contents->GetRenderWidgetHostView(),
                                  initial_pos);
  }
  widget_host_view->GetRenderWidgetHost()->Init();
  return widget_host_view;
}
