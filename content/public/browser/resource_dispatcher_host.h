// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
#pragma once

#include "base/callback_forward.h"
#include "content/public/browser/download_id.h"
#include "net/base/net_errors.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceContext;
class ResourceDispatcherHostDelegate;
struct DownloadSaveInfo;

class CONTENT_EXPORT ResourceDispatcherHost {
 public:
  typedef base::Callback<void(DownloadId, net::Error)> DownloadStartedCallback;

  // Returns the singleton instance of the ResourceDispatcherHost.
  static ResourceDispatcherHost* Get();

  // This does not take ownership of the delegate. It is expected that the
  // delegate have a longer lifetime than the ResourceDispatcherHost.
  virtual void SetDelegate(ResourceDispatcherHostDelegate* delegate) = 0;

  // Controls whether third-party sub-content can pop-up HTTP basic auth
  // dialog boxes.
  virtual void SetAllowCrossOriginAuthPrompt(bool value) = 0;

  // Initiates a download by explicit request of the renderer, e.g. due to
  // alt-clicking a link.  If the download is started, |started_callback| will
  // be called on the UI thread with the DownloadId; otherwise an error code
  // will be returned.  |is_content_initiated| is used to indicate that
  // the request was generated from a web page, and hence may not be
  // as trustworthy as a browser generated request.
  virtual net::Error BeginDownload(
      scoped_ptr<net::URLRequest> request,
      bool is_content_initiated,
      ResourceContext* context,
      int child_id,
      int route_id,
      bool prefer_cache,
      const DownloadSaveInfo& save_info,
      const DownloadStartedCallback& started_callback) = 0;

  // Clears the ResourceDispatcherHostLoginDelegate associated with the request.
  virtual void ClearLoginDelegateForRequest(net::URLRequest* request) = 0;

  // Marks the request as "parked". This happens if a request is
  // redirected cross-site and needs to be resumed by a new render view.
  virtual void MarkAsTransferredNavigation(net::URLRequest* request) = 0;

 protected:
  virtual ~ResourceDispatcherHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
