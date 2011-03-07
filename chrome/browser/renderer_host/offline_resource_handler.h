// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_OFFLINE_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_OFFLINE_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/chromeos/offline/offline_load_page.h"
#include "content/browser/renderer_host/resource_handler.h"
#include "net/base/completion_callback.h"

class MessageLoop;
class ResourceDispatcherHost;

namespace net {
class URLRequest;
}  // namespace net

// Used to show an offline interstitial page when the network is not available.
class OfflineResourceHandler : public ResourceHandler,
                               public chromeos::OfflineLoadPage::Delegate {
 public:
  OfflineResourceHandler(ResourceHandler* handler,
                         int host_id,
                         int render_view_id,
                         ResourceDispatcherHost* rdh,
                         net::URLRequest* request);
  virtual ~OfflineResourceHandler();

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  virtual bool OnRequestRedirected(int request_id, const GURL& new_url,
                           ResourceResponse* response, bool* defer);
  virtual bool OnResponseStarted(int request_id, ResourceResponse* response);
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer);
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                          int min_size);
  virtual bool OnReadCompleted(int request_id, int* bytes_read);
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info);
  virtual void OnRequestClosed();

  // chromeos::OfflineLoadPage::Delegate
  virtual void OnBlockingPageComplete(bool proceed);

 private:
  // Erease the state assocaited with a deferred load request.
  void ClearRequestInfo();
  bool IsRemote(const GURL& url) const;

  // Resume the deferred load request.
  void Resume();

  // Tells if chrome should show the offline page.
  bool ShouldShowOfflinePage(const GURL& url) const;

  // Shows the offline interstitinal page in UI thread.
  void ShowOfflinePage();

  // A callback to tell if an appcache exists.
  void OnCanHandleOfflineComplete(int rv);

  scoped_refptr<ResourceHandler> next_handler_;

  int process_host_id_;
  int render_view_id_;
  ResourceDispatcherHost* rdh_;
  net::URLRequest* request_;

  // The state for deferred load quest.
  int deferred_request_id_;
  GURL deferred_url_;

  scoped_refptr<net::CancelableCompletionCallback<OfflineResourceHandler> >
      appcache_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(OfflineResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_OFFLINE_RESOURCE_HANDLER_H_
