// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/websharedworkerrepository_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/websharedworker_proxy.h"

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
