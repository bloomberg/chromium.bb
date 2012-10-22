// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "content/renderer/websharedworkerrepository_impl.h"

#include "content/common/child_thread.h"
#include "content/common/view_messages.h"
#include "content/renderer/websharedworker_proxy.h"

namespace content {

WebSharedWorkerRepositoryImpl::WebSharedWorkerRepositoryImpl() {}

WebSharedWorkerRepositoryImpl::~WebSharedWorkerRepositoryImpl() {}

void WebSharedWorkerRepositoryImpl::addSharedWorker(
    WebKit::WebSharedWorker* worker, DocumentID document) {
  shared_worker_parents_.insert(document);
}

void WebSharedWorkerRepositoryImpl::documentDetached(DocumentID document) {
  DocumentSet::iterator iter = shared_worker_parents_.find(document);
  if (iter != shared_worker_parents_.end()) {
    // Notify the browser process that the document has shut down.
    ChildThread::current()->Send(new ViewHostMsg_DocumentDetached(document));
    shared_worker_parents_.erase(iter);
  }
}

bool WebSharedWorkerRepositoryImpl::hasSharedWorkers(DocumentID document) {
  return shared_worker_parents_.find(document) != shared_worker_parents_.end();
}

}  // namespace content
