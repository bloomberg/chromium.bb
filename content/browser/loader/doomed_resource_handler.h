// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_DOOMED_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_DOOMED_RESOURCE_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/resource_handler.h"

namespace content {

// ResourceHandler that DCHECKs on all events but canceling and failing of
// requests while activated for a URLRequest.
class DoomedResourceHandler : public ResourceHandler {
 public:
  // As the DoomedResourceHandler is constructed and substituted from code
  // of another ResourceHandler, we need to make sure that this other handler
  // does not lose its last reference and gets destroyed by being substituted.
  // Therefore, we retain a reference to |old_handler| that prevents the
  // destruction.
  explicit DoomedResourceHandler(scoped_ptr<ResourceHandler> old_handler);
  virtual ~DoomedResourceHandler();

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& new_url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id,
                               int bytes_read,
                               bool* defer) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnDataDownloaded(int request_id,
                                int bytes_downloaded) OVERRIDE;

 private:
  scoped_ptr<ResourceHandler> old_handler_;

  DISALLOW_COPY_AND_ASSIGN(DoomedResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_DOOMED_RESOURCE_HANDLER_H_
