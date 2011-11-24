// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_OFFLINE_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_OFFLINE_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/offline/offline_load_page.h"
#include "content/browser/renderer_host/resource_handler.h"
#include "net/base/completion_callback.h"

class ChromeAppCacheService;
class ResourceDispatcherHost;

namespace net {
class URLRequest;
}  // namespace net

// Used to show an offline interstitial page when the network is not available.
class OfflineResourceHandler : public ResourceHandler {
 public:
  OfflineResourceHandler(ResourceHandler* handler,
                         int host_id,
                         int render_view_id,
                         ResourceDispatcherHost* rdh,
                         net::URLRequest* request,
                         ChromeAppCacheService* appcache_service);
  virtual ~OfflineResourceHandler();

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id, uint64 position,
      uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id, const GURL& new_url,
      content::ResourceResponse* response, bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
      content::ResourceResponse* response) OVERRIDE;
  virtual bool OnWillStart(int request_id, const GURL& url,
      bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
      int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id, int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

  // OfflineLoadPage callback.
  void OnBlockingPageComplete(bool proceed);

 private:
  // Erase the state assocaited with a deferred load request.
  void ClearRequestInfo();
  bool IsRemote(const GURL& url) const;

  // Resume the deferred load request.
  void Resume();

  // True if chrome should show the offline page.
  bool ShouldShowOfflinePage(const GURL& url) const;

  // Shows the offline interstitial page on the UI thread.
  void ShowOfflinePage();

  // A callback to tell if an appcache exists.
  void OnCanHandleOfflineComplete(int rv);

  scoped_refptr<ResourceHandler> next_handler_;

  int process_host_id_;
  int render_view_id_;
  ResourceDispatcherHost* rdh_;
  net::URLRequest* request_;
  ChromeAppCacheService* const appcache_service_;

  // The state for deferred load quest.
  int deferred_request_id_;
  GURL deferred_url_;

  scoped_refptr<net::CancelableOldCompletionCallback<OfflineResourceHandler> >
      appcache_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(OfflineResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_OFFLINE_RESOURCE_HANDLER_H_
