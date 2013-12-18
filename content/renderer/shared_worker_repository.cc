// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker_repository.h"

#include "content/child/child_thread.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/websharedworker_proxy.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace content {

SharedWorkerRepository::SharedWorkerRepository(RenderFrameImpl* render_frame)
    : RenderFrameObserver(render_frame) {
}

SharedWorkerRepository::~SharedWorkerRepository() {}

void SharedWorkerRepository::WebFrameCreated(blink::WebFrame* frame) {
  frame->setSharedWorkerRepositoryClient(this);
}

blink::WebSharedWorkerConnector*
SharedWorkerRepository::createSharedWorkerConnector(
    const blink::WebURL& url,
    const blink::WebString& name,
    DocumentID document_id) {
  int route_id = MSG_ROUTING_NONE;
  bool exists = false;
  bool url_mismatch = false;
  ViewHostMsg_CreateWorker_Params params;
  params.url = url;
  params.name = name;
  params.document_id = document_id;
  params.render_view_route_id = render_frame()->GetRenderView()->GetRoutingID();
  params.render_frame_route_id = render_frame()->GetRoutingID();
  params.route_id = MSG_ROUTING_NONE;
  params.script_resource_appcache_id = 0;
  Send(new ViewHostMsg_LookupSharedWorker(
      params, &exists, &route_id, &url_mismatch));
  if (url_mismatch)
    return NULL;
  documents_with_workers_.insert(document_id);
  return new WebSharedWorkerProxy(ChildThread::current(),
                                  document_id,
                                  exists,
                                  route_id,
                                  params.render_view_route_id,
                                  params.render_frame_route_id);
}

void SharedWorkerRepository::documentDetached(DocumentID document) {
  std::set<DocumentID>::iterator iter = documents_with_workers_.find(document);
  if (iter != documents_with_workers_.end()) {
    // Notify the browser process that the document has shut down.
    Send(new ViewHostMsg_DocumentDetached(document));
    documents_with_workers_.erase(iter);
  }
}

}  // namespace content
