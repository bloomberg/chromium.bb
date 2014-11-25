// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_BUFFERED_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_BUFFERED_RESOURCE_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_controller.h"

namespace net {
class URLRequest;
}

namespace content {
class PluginService;
class ResourceDispatcherHostImpl;
struct WebPluginInfo;

// Used to buffer a request until enough data has been received.
class CONTENT_EXPORT BufferedResourceHandler
    : public LayeredResourceHandler,
      public ResourceController {
 public:
  // If ENABLE_PLUGINS is defined, |plugin_service| must not be NULL.
  BufferedResourceHandler(scoped_ptr<ResourceHandler> next_handler,
                          ResourceDispatcherHostImpl* host,
                          PluginService* plugin_service,
                          net::URLRequest* request);
  ~BufferedResourceHandler() override;

 private:
  // ResourceHandler implementation:
  void SetController(ResourceController* controller) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;

  // ResourceController implementation:
  void Resume() override;
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;

  bool ProcessResponse(bool* defer);

  bool ShouldSniffContent();
  bool DetermineMimeType();
  bool SelectNextHandler(bool* defer);
  bool UseAlternateNextHandler(scoped_ptr<ResourceHandler> handler,
                               const std::string& payload_for_old_handler);

  bool ReplayReadCompleted(bool* defer);
  void CallReplayReadCompleted();

  bool MustDownload();
  bool HasSupportingPlugin(bool* is_stale);

  // Copies data from |read_buffer_| to |next_handler_|.
  bool CopyReadBufferToNextHandler();

  // Called on the IO thread once the list of plugins has been loaded.
  void OnPluginsLoaded(const std::vector<WebPluginInfo>& plugins);

  enum State {
    STATE_STARTING,

    // In this state, we are filling read_buffer_ with data for the purpose
    // of sniffing the mime type of the response.
    STATE_BUFFERING,

    // In this state, we are select an appropriate downstream ResourceHandler
    // based on the mime type of the response.  We are also potentially waiting
    // for plugins to load so that we can determine if a plugin is available to
    // handle the mime type.
    STATE_PROCESSING,

    // In this state, we are replaying buffered events (OnResponseStarted and
    // OnReadCompleted) to the downstream ResourceHandler.
    STATE_REPLAYING,

    // In this state, we are just a blind pass-through ResourceHandler.
    STATE_STREAMING
  };
  State state_;

  scoped_refptr<ResourceResponse> response_;
  ResourceDispatcherHostImpl* host_;
  PluginService* plugin_service_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  int bytes_read_;

  bool must_download_;
  bool must_download_is_set_;

  base::WeakPtrFactory<BufferedResourceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_BUFFERED_RESOURCE_HANDLER_H_
