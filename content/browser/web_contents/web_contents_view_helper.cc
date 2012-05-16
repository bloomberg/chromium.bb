// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_helper.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"

using content::RenderViewHostImpl;
using content::RenderWidgetHost;
using content::RenderWidgetHostImpl;
using content::RenderWidgetHostView;
using content::RenderWidgetHostViewPort;
using content::SiteInstance;

WebContentsViewHelper::WebContentsViewHelper() {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

WebContentsViewHelper::~WebContentsViewHelper() {}

void WebContentsViewHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED);
  RenderWidgetHost* host = content::Source<RenderWidgetHost>(source).ptr();
  for (PendingWidgetViews::iterator i = pending_widget_views_.begin();
       i != pending_widget_views_.end(); ++i) {
    if (host->GetView() == i->second) {
      pending_widget_views_.erase(i);
      break;
    }
  }
}

WebContentsImpl* WebContentsViewHelper::CreateNewWindow(
    WebContentsImpl* opener,
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  bool should_create = true;
  if (opener->GetDelegate()) {
    should_create = opener->GetDelegate()->ShouldCreateWebContents(
        opener,
        route_id,
        params.window_container_type,
        params.frame_name,
        params.target_url);
  }

  if (!should_create)
    return NULL;

  // We usually create the new window in the same BrowsingInstance (group of
  // script-related windows), by passing in the current SiteInstance.  However,
  // if the opener is being suppressed, we create a new SiteInstance in its own
  // BrowsingInstance.
  scoped_refptr<SiteInstance> site_instance =
      params.opener_suppressed ?
      SiteInstance::Create(opener->GetBrowserContext()) :
      opener->GetSiteInstance();

  // Create the new web contents. This will automatically create the new
  // WebContentsView. In the future, we may want to create the view separately.
  WebContentsImpl* new_contents =
      new WebContentsImpl(opener->GetBrowserContext(),
                          site_instance,
                          route_id,
                          opener,
                          params.opener_suppressed ? NULL : opener,
                          NULL);
  new_contents->set_opener_web_ui_type(
      opener->GetWebUITypeForCurrentState());

  if (!params.opener_suppressed) {
    content::WebContentsView* new_view = new_contents->GetView();

    // TODO(brettw): It seems bogus that we have to call this function on the
    // newly created object and give it one of its own member variables.
    new_view->CreateViewForWidget(new_contents->GetRenderViewHost());

    // Save the created window associated with the route so we can show it
    // later.
    DCHECK_NE(MSG_ROUTING_NONE, route_id);
    pending_contents_[route_id] = new_contents;
  }

  if (opener->GetDelegate()) {
    opener->GetDelegate()->WebContentsCreated(opener,
                                              params.opener_frame_id,
                                              params.target_url,
                                              new_contents);
  }

  if (params.opener_suppressed) {
    // When the opener is suppressed, the original renderer cannot access the
    // new window.  As a result, we need to show and navigate the window here.
    gfx::Rect initial_pos;
    opener->AddNewContents(new_contents,
                           params.disposition,
                           initial_pos,
                           params.user_gesture);

    content::OpenURLParams open_params(params.target_url, content::Referrer(),
                                       CURRENT_TAB,
                                       content::PAGE_TRANSITION_LINK,
                                       true /* is_renderer_initiated */);
    new_contents->OpenURL(open_params);
  }

  return new_contents;
}

RenderWidgetHostView* WebContentsViewHelper::CreateNewWidget(
    WebContentsImpl* web_contents,
    int route_id,
    bool is_fullscreen,
    WebKit::WebPopupType popup_type) {
  content::RenderProcessHost* process = web_contents->GetRenderProcessHost();
  RenderWidgetHostImpl* widget_host =
      new RenderWidgetHostImpl(web_contents, process, route_id);
  RenderWidgetHostViewPort* widget_view =
      RenderWidgetHostViewPort::CreateViewForWidget(widget_host);
  if (!is_fullscreen) {
    // Popups should not get activated.
    widget_view->SetPopupType(popup_type);
  }
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[route_id] = widget_view;
  return widget_view;
}

WebContentsImpl* WebContentsViewHelper::GetCreatedWindow(int route_id) {
  PendingContents::iterator iter = pending_contents_.find(route_id);

  // Certain systems can block the creation of new windows. If we didn't succeed
  // in creating one, just return NULL.
  if (iter == pending_contents_.end()) {
    return NULL;
  }

  WebContentsImpl* new_contents = iter->second;
  pending_contents_.erase(route_id);

  if (!new_contents->GetRenderProcessHost()->HasConnection() ||
      !new_contents->GetRenderViewHost()->GetView())
    return NULL;

  // TODO(brettw): It seems bogus to reach into here and initialize the host.
  static_cast<RenderViewHostImpl*>(new_contents->GetRenderViewHost())->Init();
  return new_contents;
}

RenderWidgetHostView* WebContentsViewHelper::GetCreatedWidget(int route_id) {
  PendingWidgetViews::iterator iter = pending_widget_views_.find(route_id);
  if (iter == pending_widget_views_.end()) {
    DCHECK(false);
    return NULL;
  }

  RenderWidgetHostView* widget_host_view = iter->second;
  pending_widget_views_.erase(route_id);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->GetProcess()->HasConnection()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return NULL;
  }

  return widget_host_view;
}

WebContentsImpl* WebContentsViewHelper::ShowCreatedWindow(
    WebContentsImpl* web_contents,
    int route_id,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  WebContentsImpl* contents = GetCreatedWindow(route_id);
  if (contents) {
    web_contents->AddNewContents(contents,
                                 disposition,
                                 initial_pos,
                                 user_gesture);
  }
  return contents;
}

RenderWidgetHostView* WebContentsViewHelper::ShowCreatedWidget(
    WebContentsImpl* web_contents,
    int route_id,
    bool is_fullscreen,
    const gfx::Rect& initial_pos) {
  if (web_contents->GetDelegate())
    web_contents->GetDelegate()->RenderWidgetShowing();

  RenderWidgetHostViewPort* widget_host_view =
      RenderWidgetHostViewPort::FromRWHV(GetCreatedWidget(route_id));
  if (is_fullscreen) {
    widget_host_view->InitAsFullscreen(web_contents->GetRenderWidgetHostView());
  } else {
    widget_host_view->InitAsPopup(web_contents->GetRenderWidgetHostView(),
                                  initial_pos);
  }
  RenderWidgetHostImpl::From(widget_host_view->GetRenderWidgetHost())->Init();
  return widget_host_view;
}
