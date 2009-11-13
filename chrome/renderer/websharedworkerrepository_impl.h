// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_WEB_SHARED_WORKER_REPOSITORY_IMPL_H_
#define CHROME_RENDERER_WEB_SHARED_WORKER_REPOSITORY_IMPL_H_

#include "third_party/WebKit/WebKit/chromium/public/WebSharedWorkerRepository.h"

#include "base/hash_tables.h"

namespace WebKit {
  class WebSharedWorker;
}

class WebSharedWorkerRepositoryImpl : public WebKit::WebSharedWorkerRepository {
 public:
    virtual void addSharedWorker(WebKit::WebSharedWorker*, DocumentID document);
    virtual void documentDetached(DocumentID document);

    // Returns true if the document has created a SharedWorker (used by the
    // WebKit code to determine if the document can be suspended).
    virtual bool hasSharedWorkers(DocumentID document);
 private:
    // The set of documents that have created a SharedWorker.
    typedef base::hash_set<DocumentID> DocumentSet;
    DocumentSet shared_worker_parents_;
};

#endif  // CHROME_RENDERER_WEB_SHARED_WORKER_REPOSITORY_IMPL_H_
