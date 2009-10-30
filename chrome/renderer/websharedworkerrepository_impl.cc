// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/websharedworkerrepository_impl.h"

WebKit::WebSharedWorker* WebSharedWorkerRepositoryImpl::lookup(
    const WebKit::WebURL& url,
    const WebKit::WebString& name,
    DocumentID document) {
    return NULL;
}

void WebSharedWorkerRepositoryImpl::documentDetached(
    DocumentID document) {
    // TODO(atwilson): Update this to call to WorkerService to shutdown any
    // associated SharedWorkers.
}

bool WebSharedWorkerRepositoryImpl::hasSharedWorkers(
    DocumentID document) {
    // TODO(atwilson): Update this when we track shared worker creation.
    return false;
}
