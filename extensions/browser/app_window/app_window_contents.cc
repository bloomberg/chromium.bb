// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_contents.h"

#include <string>
#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

AppWindowContentsImpl::AppWindowContentsImpl(AppWindow* host) : host_(host) {}

AppWindowContentsImpl::~AppWindowContentsImpl() {}

void AppWindowContentsImpl::Initialize(content::BrowserContext* context,
                                       const GURL& url) {
  url_ = url;

  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(context, this));

  web_contents_.reset(
      content::WebContents::Create(content::WebContents::CreateParams(
          context, content::SiteInstance::CreateForURL(context, url_))));

  Observe(web_contents_.get());
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

  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
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

WindowController* AppWindowContentsImpl::GetExtensionWindowController() const {
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
    const std::vector<DraggableRegion>& regions) {
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

}  // namespace extensions
