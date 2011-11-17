// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "content/browser/renderer_host/resource_handler.h"

class ResourceDispatcherHost;

namespace net {
class URLRequest;
}  // namespace net

namespace webkit {
struct WebPluginInfo;
}

// Used to buffer a request until enough data has been received.
class BufferedResourceHandler : public ResourceHandler {
 public:
  BufferedResourceHandler(ResourceHandler* handler,
                          ResourceDispatcherHost* host,
                          net::URLRequest* request);

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& new_url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id, int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

 private:
  virtual ~BufferedResourceHandler();

  // Returns true if we should delay OnResponseStarted forwarding.
  bool DelayResponse();

  // Returns true if there is enough information to process the DocType.
  bool DidBufferEnough(int bytes_read);

  // Returns true if we have to keep buffering data.
  bool KeepBuffering(int bytes_read);

  // Sends a pending OnResponseStarted notification. |in_complete| is true if
  // this is invoked from |OnResponseCompleted|.
  bool CompleteResponseStarted(int request_id, bool in_complete);

  // Returns true if we have to wait until the plugin list is generated.
  bool ShouldWaitForPlugins();

  // A test to determining whether the request should be forwarded to the
  // download thread.  If need_plugin_list was passed in and was set to true,
  // that means that the check couldn't be fully done because the plugins aren't
  // loaded.  The function should be called again after the plugin list is
  // loaded.
  bool ShouldDownload(bool* need_plugin_list);

  // Informs the original ResourceHandler |real_handler_| that the response will
  // be handled entirely by the new ResourceHandler |handler|.
  // A reference to |handler| is acquired.
  void UseAlternateResourceHandler(int request_id, ResourceHandler* handler);

  // Called on the IO thread once the list of plugins has been loaded.
  void OnPluginsLoaded(const std::vector<webkit::WebPluginInfo>& plugins);

  scoped_refptr<ResourceHandler> real_handler_;
  scoped_refptr<ResourceResponse> response_;
  ResourceDispatcherHost* host_;
  net::URLRequest* request_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> my_buffer_;
  int read_buffer_size_;
  int bytes_read_;
  bool sniff_content_;
  bool wait_for_plugins_;
  bool buffering_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceHandler);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
