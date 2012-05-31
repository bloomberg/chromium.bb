// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/layered_resource_handler.h"

namespace net {
class URLRequest;
}  // namespace net

namespace webkit {
struct WebPluginInfo;
}

namespace content {
class ResourceDispatcherHostImpl;

// Used to buffer a request until enough data has been received.
class BufferedResourceHandler
    : public LayeredResourceHandler,
      public base::SupportsWeakPtr<BufferedResourceHandler> {
 public:
  BufferedResourceHandler(scoped_ptr<ResourceHandler> next_handler,
                          ResourceDispatcherHostImpl* host,
                          net::URLRequest* request);
  virtual ~BufferedResourceHandler();

  // ResourceHandler implementation:
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id, int* bytes_read,
                               bool* defer) OVERRIDE;

 private:
  // Returns true if we should delay OnResponseStarted forwarding.
  bool DelayResponse();

  // Returns true if there is enough information to process the DocType.
  bool DidBufferEnough(int bytes_read);

  // Returns true if we have to keep buffering data.
  bool KeepBuffering(int bytes_read);

  // Sends a pending OnResponseStarted notification.
  bool CompleteResponseStarted(int request_id, bool* defer);

  // Returns true if we have to wait until the plugin list is generated.
  bool ShouldWaitForPlugins();

  // A test to determining whether the request should be forwarded to the
  // download thread.  If need_plugin_list was passed in and was set to true,
  // that means that the check couldn't be fully done because the plugins aren't
  // loaded.  The function should be called again after the plugin list is
  // loaded.
  bool ShouldDownload(bool* need_plugin_list);

  // Informs the original ResourceHandler |next_handler_| that the response
  // will be handled entirely by the new ResourceHandler |handler|.  A
  // reference to |handler| is acquired.  Returns false to indicate an error,
  // which will result in the request being cancelled.
  bool UseAlternateResourceHandler(int request_id,
                                   scoped_ptr<ResourceHandler> handler,
                                   bool* defer);

  // Forwards any queued events to |next_handler_|.  Returns false to indicate
  // an error, which will result in the request being cancelled.
  bool ForwardPendingEventsToNextHandler(int request_id, bool* defer);

  // Copies data from |read_buffer_| to |next_handler_|.
  void CopyReadBufferToNextHandler(int request_id);

  // Called on the IO thread once the list of plugins has been loaded.
  void OnPluginsLoaded(const std::vector<webkit::WebPluginInfo>& plugins);

  scoped_refptr<ResourceResponse> response_;
  ResourceDispatcherHostImpl* host_;
  net::URLRequest* request_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> my_buffer_;
  int read_buffer_size_;
  int bytes_read_;
  bool sniff_content_;
  bool wait_for_plugins_;
  bool deferred_waiting_for_plugins_;
  bool buffering_;
  bool next_handler_needs_response_started_;
  bool next_handler_needs_will_read_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
