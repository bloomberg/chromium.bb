// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_fullscreen_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

using content::WebContents;

RenderViewHostDelegateViewHelper::RenderViewHostDelegateViewHelper() {
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 content::NotificationService::AllSources());
}

RenderViewHostDelegateViewHelper::~RenderViewHostDelegateViewHelper() {}

void RenderViewHostDelegateViewHelper::Observe(
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

BackgroundContents*
RenderViewHostDelegateViewHelper::MaybeCreateBackgroundContents(
    int route_id,
    Profile* profile,
    SiteInstance* site,
    const GURL& opener_url,
    const string16& frame_name) {
  ExtensionService* extensions_service = profile->GetExtensionService();

  if (!opener_url.is_valid() ||
      frame_name.empty() ||
      !extensions_service ||
      !extensions_service->is_ready())
    return NULL;

  // Only hosted apps have web extents, so this ensures that only hosted apps
  // can create BackgroundContents. We don't have to check for background
  // permission as that is checked in RenderMessageFilter when the CreateWindow
  // message is processed.
  const Extension* extension =
      extensions_service->extensions()->GetHostedAppByURL(
          ExtensionURLInfo(opener_url));
  if (!extension)
    return NULL;

  // No BackgroundContents allowed if BackgroundContentsService doesn't exist.
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile);
  if (!service)
    return NULL;

  // Ensure that we're trying to open this from the extension's process.
  extensions::ProcessMap* process_map = extensions_service->process_map();
  if (!site->GetProcess() ||
      !process_map->Contains(extension->id(), site->GetProcess()->GetID())) {
    return NULL;
  }

  // Only allow a single background contents per app. If one already exists,
  // close it (even if it was specified in the manifest).
  BackgroundContents* existing =
      service->GetAppBackgroundContents(ASCIIToUTF16(extension->id()));
  if (existing) {
    DLOG(INFO) << "Closing existing BackgroundContents for " << opener_url;
    delete existing;
  }

  // Passed all the checks, so this should be created as a BackgroundContents.
  return service->CreateBackgroundContents(site, route_id, profile, frame_name,
                                           ASCIIToUTF16(extension->id()));
}

TabContents* RenderViewHostDelegateViewHelper::CreateNewWindow(
    int route_id,
    Profile* profile,
    SiteInstance* site,
    WebUI::TypeID webui_type,
    RenderViewHostDelegate* opener,
    WindowContainerType window_container_type,
    const string16& frame_name) {
  if (window_container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    BackgroundContents* contents = MaybeCreateBackgroundContents(
        route_id,
        profile,
        site,
        opener->GetURL(),
        frame_name);
    if (contents) {
      pending_contents_[route_id] =
          contents->web_contents()->GetRenderViewHost();
      return NULL;
    }
  }

  TabContents* base_tab_contents = opener->GetAsTabContents();

  // Do not create the new TabContents if the opener is a prerender TabContents.
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(base_tab_contents)) {
    return NULL;
  }

  // Create the new web contents. This will automatically create the new
  // TabContentsView. In the future, we may want to create the view separately.
  TabContents* new_contents =
      new TabContents(profile,
                      site,
                      route_id,
                      base_tab_contents,
                      NULL);
  new_contents->set_opener_web_ui_type(webui_type);
  TabContentsView* new_view = new_contents->GetView();

  // TODO(brettw) it seems bogus that we have to call this function on the
  // newly created object and give it one of its own member variables.
  new_view->CreateViewForWidget(new_contents->GetRenderViewHost());

  // Save the created window associated with the route so we can show it later.
  pending_contents_[route_id] = new_contents->GetRenderViewHost();
  return new_contents;
}

RenderWidgetHostView* RenderViewHostDelegateViewHelper::CreateNewWidget(
    int route_id, WebKit::WebPopupType popup_type,
    content::RenderProcessHost* process) {
  RenderWidgetHost* widget_host =
      new RenderWidgetHost(process, route_id);
  RenderWidgetHostView* widget_view =
      RenderWidgetHostView::CreateViewForWidget(widget_host);
  // Popups should not get activated.
  widget_view->set_popup_type(popup_type);
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[route_id] = widget_view;
  return widget_view;
}

RenderWidgetHostView*
RenderViewHostDelegateViewHelper::CreateNewFullscreenWidget(
    int route_id, content::RenderProcessHost* process) {
  RenderWidgetFullscreenHost* fullscreen_widget_host =
      new RenderWidgetFullscreenHost(process, route_id);
  RenderWidgetHostView* widget_view =
      RenderWidgetHostView::CreateViewForWidget(fullscreen_widget_host);
  pending_widget_views_[route_id] = widget_view;
  return widget_view;
}

TabContents* RenderViewHostDelegateViewHelper::GetCreatedWindow(int route_id) {
  PendingContents::iterator iter = pending_contents_.find(route_id);

  // Certain systems can block the creation of new windows. If we didn't succeed
  // in creating one, just return NULL.
  if (iter == pending_contents_.end()) {
    return NULL;
  }

  RenderViewHost* new_rvh = iter->second;
  pending_contents_.erase(route_id);

  // The renderer crashed or it is a TabContents and has no view.
  if (!new_rvh->process()->HasConnection() ||
      (new_rvh->delegate()->GetAsTabContents() && !new_rvh->view()))
    return NULL;

  // TODO(brettw) this seems bogus to reach into here and initialize the host.
  new_rvh->Init();
  return new_rvh->delegate()->GetAsTabContents();
}

RenderWidgetHostView* RenderViewHostDelegateViewHelper::GetCreatedWidget(
    int route_id) {
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

TabContents* RenderViewHostDelegateViewHelper::CreateNewWindowFromTabContents(
    WebContents* web_contents,
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  TabContents* new_contents = CreateNewWindow(
      route_id,
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      web_contents->GetSiteInstance(),
      web_contents->GetWebUITypeForCurrentState(),
      static_cast<TabContents*>(web_contents),
      params.window_container_type,
      params.frame_name);

  if (new_contents) {
    if (web_contents->GetDelegate())
      web_contents->GetDelegate()->WebContentsCreated(new_contents);

    RetargetingDetails details;
    details.source_web_contents = web_contents;
    details.source_frame_id = params.opener_frame_id;
    details.target_url = params.target_url;
    details.target_web_contents = new_contents;
    details.not_yet_in_tabstrip = true;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_RETARGETING,
        content::Source<Profile>(
            Profile::FromBrowserContext(web_contents->GetBrowserContext())),
        content::Details<RetargetingDetails>(&details));
  } else {
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_CREATING_NEW_WINDOW_CANCELLED,
        content::Source<WebContents>(web_contents),
        content::Details<const ViewHostMsg_CreateWindow_Params>(&params));
  }

  return new_contents;
}

TabContents* RenderViewHostDelegateViewHelper::ShowCreatedWindow(
    WebContents* web_contents,
    int route_id,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  WebContents* contents = GetCreatedWindow(route_id);
  if (contents) {
    web_contents->AddNewContents(
        contents, disposition, initial_pos, user_gesture);
  }
  return static_cast<TabContents*>(contents);
}

RenderWidgetHostView* RenderViewHostDelegateViewHelper::ShowCreatedWidget(
    WebContents* web_contents, int route_id, const gfx::Rect& initial_pos) {
  if (web_contents->GetDelegate())
    web_contents->GetDelegate()->RenderWidgetShowing();

  RenderWidgetHostView* widget_host_view = GetCreatedWidget(route_id);
  widget_host_view->InitAsPopup(web_contents->GetRenderWidgetHostView(),
                                initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
  return widget_host_view;
}

RenderWidgetHostView*
    RenderViewHostDelegateViewHelper::ShowCreatedFullscreenWidget(
        content::WebContents* web_contents, int route_id) {
  if (web_contents->GetDelegate())
    web_contents->GetDelegate()->RenderWidgetShowing();

  RenderWidgetHostView* widget_host_view = GetCreatedWidget(route_id);
  widget_host_view->InitAsFullscreen(web_contents->GetRenderWidgetHostView());
  widget_host_view->GetRenderWidgetHost()->Init();
  return widget_host_view;
}
