// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/websharedworkerrepository_impl.h"

#include "chrome/renderer/websharedworker_proxy.h"

void WebSharedWorkerRepositoryImpl::addSharedWorker(
    WebKit::WebSharedWorker* worker, DocumentID document) {
  // TODO(atwilson): Track shared worker creation here.
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
