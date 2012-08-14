// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DuplicateContentResourceHandler keeps a hash of resources seen and based on
// url. It is an experiment meant to simulate cache hits for both a url-based
// and a content-based cache.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DUPLICATE_CONTENT_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_DUPLICATE_CONTENT_RESOURCE_HANDLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/layered_resource_handler.h"
#include "third_party/smhasher/src/PMurHash.h"
#include "webkit/glue/resource_type.h"

namespace net {
class IOBuffer;
class URLRequest;
class URLRequestStatus;
}

namespace content {

class DuplicateContentResourceHandler: public LayeredResourceHandler {
  public:
  DuplicateContentResourceHandler(scoped_ptr<ResourceHandler> next_handler,
                                  ResourceType::Type resource_type,
                                  net::URLRequest* request);
  virtual ~DuplicateContentResourceHandler();

 private:
  // ResourceHandler implementation
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

  void RecordContentMetrics();

  ResourceType::Type resource_type_;
  int bytes_read_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  net::URLRequest* request_;

  // These values are temporary values that are used in each digest of the
  // inputs for the incremental hash (PMurHash).
  MH_UINT32 pmurhash_ph1_;
  MH_UINT32 pmurhash_pcarry_;

  // Used to track if it's a request we're interested in. If it's not, then we
  // early out to avoid calculating the hash unnecessarily.
  bool track_request_;

  DISALLOW_COPY_AND_ASSIGN(DuplicateContentResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DUPLICATE_CONTENT_RESOURCE_HANDLER_H_

