// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window_contents.h"

#include <utility>
#include <string>

#include "apps/ui/native_app_window.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/extensions/api/app_window.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/common/extension_messages.h"

namespace app_window = extensions::api::app_window;

namespace apps {

AppWindowContentsImpl::AppWindowContentsImpl(AppWindow* host) : host_(host) {}

AppWindowContentsImpl::~AppWindowContentsImpl() {}

void AppWindowContentsImpl::Initialize(content::BrowserContext* context,
                                       const GURL& url) {
  url_ = url;

  extension_function_dispatcher_.reset(
      new extensions::ExtensionFunctionDispatcher(context, this));

  web_contents_.reset(
      content::WebContents::Create(content::WebContents::CreateParams(
          context, content::SiteInstance::CreateForURL(context, url_))));

  content::WebContentsObserver::Observe(web_contents_.get());
  web_contents_->GetMutableRendererPrefs()->
      browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();
}

void AppWindowContentsImpl::LoadContents(int32 creator_process_id) {
  // If the new view is in the same process as the creator, block the created
  // RVH from loading anything until the background page has had a chance to
  // do any initialization it wants. If it's a different process, the new RVH
  // shouldn't communicate with the background page anyway (e.g. sandboxed).
  if (web_contents_->GetRenderViewHost()->GetProcess()->GetID() ==
      creator_process_id) {
    SuspendRenderViewHost(web_contents_->GetRenderViewHost());
  } else {
    VLOG(1) << "AppWindow created in new process ("
            << web_contents_->GetRenderViewHost()->GetProcess()->GetID()
            << ") != creator (" << creator_process_id << "). Routing disabled.";
  }

  // TODO(jeremya): there's a bug where navigating a web contents to an
  // extension URL causes it to create a new RVH and discard the old (perfectly
  // usable) one. To work around this, we watch for a
  // NOTIFICATION_RENDER_VIEW_HOST_CHANGED message from the web contents (which
  // will be sent during LoadURL) and suspend resource requests on the new RVH
  // to ensure that we block the new RVH from loading anything. It should be
  // okay to remove the NOTIFICATION_RENDER_VIEW_HOST_CHANGED registration once
  // http://crbug.com/123007 is fixed.
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::WebContents>(web_contents()));
  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
  registrar_.RemoveAll();
}

void AppWindowContentsImpl::NativeWindowChanged(
    NativeAppWindow* native_app_window) {
  base::ListValue args;
  base::DictionaryValue* dictionary = new base::DictionaryValue();
  args.Append(dictionary);
  host_->GetSerializedState(dictionary);

  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_MessageInvoke(rvh->GetRoutingID(),
                                           host_->extension_id(),
                                           "app.window",
                                           "updateAppWindowProperties",
                                           args,
                                           false));
}

void AppWindowContentsImpl::NativeWindowClosed() {
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_AppWindowClosed(rvh->GetRoutingID()));
}

void AppWindowContentsImpl::DispatchWindowShownForTests() const {
  base::ListValue args;
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ExtensionMsg_MessageInvoke(rvh->GetRoutingID(),
                                           host_->extension_id(),
                                           "app.window",
                                           "appWindowShownForTests",
                                           args,
                                           false));
}

content::WebContents* AppWindowContentsImpl::GetWebContents() const {
  return web_contents_.get();
}

void AppWindowContentsImpl::Observe(
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

bool AppWindowContentsImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppWindowContentsImpl, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_UpdateDraggableRegions,
                        UpdateDraggableRegions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

extensions::WindowController*
AppWindowContentsImpl::GetExtensionWindowController() const {
  return NULL;
}

content::WebContents* AppWindowContentsImpl::GetAssociatedWebContents() const {
  return web_contents_.get();
}

void AppWindowContentsImpl::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(
      params, web_contents_->GetRenderViewHost());
}

void AppWindowContentsImpl::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  host_->UpdateDraggableRegions(regions);
}

void AppWindowContentsImpl::SuspendRenderViewHost(
    content::RenderViewHost* rvh) {
  DCHECK(rvh);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&content::ResourceDispatcherHost::BlockRequestsForRoute,
                 base::Unretained(content::ResourceDispatcherHost::Get()),
                 rvh->GetProcess()->GetID(), rvh->GetRoutingID()));
}

}  // namespace apps
