// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_INTERCEPTING_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_INTERCEPTING_RESOURCE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/browser/loader/resource_handler.h"
#include "content/common/content_export.h"
#include "net/base/io_buffer.h"

namespace net {
class URLRequest;
}

namespace content {

// ResourceHandler that initiates special handling of the response if needed,
// based on the response's MIME type (starts downloads, sends data to some
// plugin types via a special channel).
class CONTENT_EXPORT InterceptingResourceHandler
    : public LayeredResourceHandler {
 public:
  InterceptingResourceHandler(std::unique_ptr<ResourceHandler> next_handler,
                              net::URLRequest* request);
  ~InterceptingResourceHandler() override;

  // ResourceHandler implementation:
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;

  // Replaces the next handler with |new_handler|, sending
  // |payload_for_old_handler| to the old handler. Must be called after
  // OnWillStart and OnRequestRedirected and before OnResponseStarted. One
  // OnWillRead call is permitted beforehand. |new_handler|'s OnWillStart and
  // OnRequestRedirected methods will not be called.
  void UseNewHandler(std::unique_ptr<ResourceHandler> new_handler,
                     const std::string& payload_for_old_handler);

  // Used in tests.
  ResourceHandler* new_handler_for_testing() const {
    return new_handler_.get();
  }

 private:
  enum class State {
    // In this state, the InterceptingResourceHandler is waiting for the mime
    // type of the response to be identified, to check if the next handler
    // should be replaced with an appropriate one.
    STARTING,

    // In this state, the InterceptingResourceHandler is waiting to copy the
    // read buffer to the next handler if it was replaced. This is needed
    // because MimeTypeResourceHandler may call OnWillRead before calling
    // OnResponseStarted, that is before the InterceptingResourceHandler
    // replaces the original ResourceHandler of the request. Therefore, the
    // data read at that time should be copied to the new ResourceHandler.
    WAITING_FOR_BUFFER_COPY,

    // In this state, the InterceptingResourceHandler has replaced its next
    // ResourceHandler if needed, and has ensured the buffered read data was
    // properly transmitted to the new ResourceHandler. The
    // InterceptingResourceHandler now acts as a pass-through ResourceHandler.
    DONE,
  };

  void SendPayloadToOldHandler();

  State state_;

  std::unique_ptr<ResourceHandler> new_handler_;
  std::string payload_for_old_handler_;

  // Result of the first read, that may have to be passed to an alternate
  // ResourceHandler instead of the original ResourceHandler.
  scoped_refptr<net::IOBuffer> first_read_buffer_;
  size_t first_read_buffer_size_;
  scoped_refptr<net::IOBuffer> first_read_buffer_copy_;

  DISALLOW_COPY_AND_ASSIGN(InterceptingResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_INTERCEPTING_RESOURCE_HANDLER_H_
