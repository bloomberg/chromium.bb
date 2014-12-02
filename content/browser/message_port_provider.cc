// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/message_port_provider.h"

#include "base/basictypes.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"

namespace content {

void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    const base::string16& source_origin,
    const base::string16& target_origin,
    const base::string16& data,
    const std::vector<int>& ports) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if (!rvh)
    return;

  ViewMsg_PostMessage_Params params;
  params.is_data_raw_string = true;
  params.data = data;
  params.source_routing_id = web_contents->GetRoutingID();
  params.source_origin = source_origin;
  params.target_origin = target_origin;
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(
          web_contents->GetRenderProcessHost());
  MessagePortMessageFilter* mf = rph->message_port_message_filter();

  if (!ports.empty()) {
    params.message_port_ids = ports;
    mf->UpdateMessagePortsWithNewRoutes(params.message_port_ids,
                                        &params.new_routing_ids);
  }
  rvh->Send(new ViewMsg_PostMessageEvent(
                web_contents->GetRenderViewHost()->GetRoutingID(),
                params));
}

void MessagePortProvider::CreateMessageChannel(WebContents* web_contents,
                                               int* port1,
                                               int* port2) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  *port1 = 0;
  *port2 = 0;

  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(
          web_contents->GetRenderProcessHost());
  MessagePortMessageFilter* mf = rph->message_port_message_filter();
  MessagePortService* msp = MessagePortService::GetInstance();
  msp->Create(mf->GetNextRoutingID(), mf, port1);
  msp->Create(mf->GetNextRoutingID(), mf, port2);
  msp->Entangle(*port1, *port2);
  msp->Entangle(*port2, *port1);
}

} // namespace content
