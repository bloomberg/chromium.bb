// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_WEB_SHARED_WORKER_REPOSITORY_IMPL_H_
#define CHROME_RENDERER_WEB_SHARED_WORKER_REPOSITORY_IMPL_H_

#include "webkit/api/public/WebSharedWorkerRepository.h"

namespace WebKit {
  class WebSharedWorker;
}

class WebSharedWorkerRepositoryImpl : public WebKit::WebSharedWorkerRepository {
    virtual WebKit::WebSharedWorker* lookup(const WebKit::WebURL& url,
                                            const WebKit::WebString& name,
                                            DocumentID document);

    virtual void documentDetached(DocumentID document);

    virtual bool hasSharedWorkers(DocumentID document);
};

#endif  // CHROME_RENDERER_WEB_SHARED_WORKER_REPOSITORY_IMPL_H_
