// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker_repository.h"

#include "content/child/child_thread.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/websharedworker_proxy.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

SharedWorkerRepository::SharedWorkerRepository(RenderViewImpl* render_view)
    : RenderViewObserver(render_view) {
  render_view->GetWebView()->setSharedWorkerRepositoryClient(this);
}

SharedWorkerRepository::~SharedWorkerRepository() {}

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
  params.render_view_route_id = render_view()->GetRoutingID();
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
                                  render_view()->GetRoutingID());
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
