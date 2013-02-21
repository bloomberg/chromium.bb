// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_window_contents.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/app_window.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"

namespace app_window = extensions::api::app_window;

AppWindowContents::AppWindowContents(ShellWindow* host)
    : host_(host) {
}

AppWindowContents::~AppWindowContents() {
}

void AppWindowContents::Initialize(Profile* profile, const GURL& url) {
  url_ = url;

  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(profile, this));

  web_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(
          profile, content::SiteInstance::CreateForURL(profile, url_))));

  content::WebContentsObserver::Observe(web_contents_.get());
  web_contents_->GetMutableRendererPrefs()->
      browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();
}

void AppWindowContents::LoadContents(int32 creator_process_id) {
  // If the new view is in the same process as the creator, block the created
  // RVH from loading anything until the background page has had a chance to
  // do any initialization it wants. If it's a different process, the new RVH
  // shouldn't communicate with the background page anyway (e.g. sandboxed).
  if (web_contents_->GetRenderViewHost()->GetProcess()->GetID() ==
      creator_process_id) {
    SuspendRenderViewHost(web_contents_->GetRenderViewHost());
  } else {
    VLOG(1) << "ShellWindow created in new process ("
            << web_contents_->GetRenderViewHost()->GetProcess()->GetID()
            << ") != creator (" << creator_process_id
            << "). Routing disabled.";
  }

  // TODO(jeremya): there's a bug where navigating a web contents to an
  // extension URL causes it to create a new RVH and discard the old
  // (perfectly usable) one. To work around this, we watch for a RVH_CHANGED
  // message from the web contents (which will be sent during LoadURL) and
  // suspend resource requests on the new RVH to ensure that we block the new
  // RVH from loading anything. It should be okay to remove the
  // NOTIFICATION_RVH_CHANGED registration once http://crbug.com/123007 is
  // fixed.
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &web_contents()->GetController()));
  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  registrar_.RemoveAll();
}

void AppWindowContents::NativeWindowChanged(
    NativeAppWindow* native_app_window) {
  ListValue args;
  DictionaryValue* dictionary = new DictionaryValue();
  args.Append(dictionary);

  gfx::Rect bounds = host_->GetClientBounds();
  app_window::Bounds update;
  update.left.reset(new int(bounds.x()));
  update.top.reset(new int(bounds.y()));
  update.width.reset(new int(bounds.width()));
  update.height.reset(new int(bounds.height()));
  dictionary->Set("bounds", update.ToValue().release());
  dictionary->SetBoolean("minimized", native_app_window->IsMinimized());
  dictionary->SetBoolean("maximized", native_app_window->IsMaximized());

  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_MessageInvoke(rvh->GetRoutingID(),
                                           host_->extension()->id(),
                                           "updateAppWindowProperties",
                                           args,
                                           GURL(),
                                           false));

}

void AppWindowContents::NativeWindowClosed() {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AppWindowClosed(rvh->GetRoutingID()));
}

content::WebContents* AppWindowContents::GetWebContents() const {
  return web_contents_.get();
}

void AppWindowContents::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED: {
      // TODO(jeremya): once http://crbug.com/123007 is fixed, we'll no longer
      // need to suspend resource requests here (the call in the constructor
      // should be enough).
      content::Details<std::pair<content::RenderViewHost*,
                                 content::RenderViewHost*> >
          host_details(details);
      if (host_details->first)
        SuspendRenderViewHost(host_details->second);
      break;
    }
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

bool AppWindowContents::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppWindowContents, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_UpdateDraggableRegions,
                        UpdateDraggableRegions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

extensions::WindowController*
AppWindowContents::GetExtensionWindowController() const {
  return NULL;
}

content::WebContents* AppWindowContents::GetAssociatedWebContents() const {
  return web_contents_.get();
}

void AppWindowContents::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(
      params, web_contents_->GetRenderViewHost());
}

void AppWindowContents::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  host_->UpdateDraggableRegions(regions);
}

void AppWindowContents::SuspendRenderViewHost(
    content::RenderViewHost* rvh) {
  DCHECK(rvh);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&content::ResourceDispatcherHost::BlockRequestsForRoute,
                 base::Unretained(content::ResourceDispatcherHost::Get()),
                 rvh->GetProcess()->GetID(), rvh->GetRoutingID()));
}
