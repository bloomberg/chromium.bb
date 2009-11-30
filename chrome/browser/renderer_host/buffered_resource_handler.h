// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_

#include <string>

#include "chrome/browser/plugin_service.h"
#include "chrome/browser/renderer_host/resource_handler.h"

class MessageLoop;
class ResourceDispatcherHost;
class URLRequest;

// Used to buffer a request until enough data has been received.
class BufferedResourceHandler : public ResourceHandler,
                                public PluginService::GetPluginListClient {
 public:
  BufferedResourceHandler(ResourceHandler* handler,
                          ResourceDispatcherHost* host,
                          URLRequest* request);

  // ResourceHandler implementation:
  bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  bool OnRequestRedirected(int request_id, const GURL& new_url,
                           ResourceResponse* response, bool* defer);
  bool OnResponseStarted(int request_id, ResourceResponse* response);
  bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                  int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);
  bool OnResponseCompleted(int request_id,
                           const URLRequestStatus& status,
                           const std::string& security_info);
  void OnRequestClosed();

 private:
  ~BufferedResourceHandler() {}

  // Returns true if we should delay OnResponseStarted forwarding.
  bool DelayResponse();

  // Returns true if there will be a need to parse the DocType of the document
  // to determine the right way to handle it.
  bool ShouldBuffer(const GURL& url, const std::string& mime_type);

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

  // Called on the IO thread once the list of plugins has been loaded.
  void OnGetPluginList(const std::vector<WebPluginInfo>& plugins);

  scoped_refptr<ResourceHandler> real_handler_;
  scoped_refptr<ResourceResponse> response_;
  ResourceDispatcherHost* host_;
  URLRequest* request_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> my_buffer_;
  int read_buffer_size_;
  int bytes_read_;
  bool sniff_content_;
  bool should_buffer_;
  bool wait_for_plugins_;
  bool buffering_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
