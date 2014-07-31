// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_RENDERER_HOST_AW_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define ANDROID_WEBVIEW_LIB_RENDERER_HOST_AW_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <map>

#include "base/lazy_instance.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"

namespace content {
class ResourceDispatcherHostLoginDelegate;
struct ResourceResponse;
}  // namespace content

namespace IPC {
class Sender;
}  // namespace IPC

namespace android_webview {

class IoThreadClientThrottle;

class AwResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  static void ResourceDispatcherHostCreated();

  // Overriden methods from ResourceDispatcherHostDelegate.
  virtual void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      content::ResourceType resource_type,
      int child_id,
      int route_id,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual void DownloadStarting(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      int child_id,
      int route_id,
      int request_id,
      bool is_content_initiated,
      bool must_download,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE;
  virtual content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      net::URLRequest* request) OVERRIDE;
  virtual bool HandleExternalProtocol(const GURL& url,
                                      int child_id,
                                      int route_id) OVERRIDE;
  virtual void OnResponseStarted(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceResponse* response,
      IPC::Sender* sender) OVERRIDE;

  virtual void OnRequestRedirected(
      const GURL& redirect_url,
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceResponse* response) OVERRIDE;

  void RemovePendingThrottleOnIoThread(IoThreadClientThrottle* throttle);

  static void OnIoThreadClientReady(int new_render_process_id,
                                    int new_render_frame_id);
  static void AddPendingThrottle(int render_process_id,
                                 int render_frame_id,
                                 IoThreadClientThrottle* pending_throttle);

 private:
  friend struct base::DefaultLazyInstanceTraits<
      AwResourceDispatcherHostDelegate>;
  AwResourceDispatcherHostDelegate();
  virtual ~AwResourceDispatcherHostDelegate();

  // These methods must be called on IO thread.
  void OnIoThreadClientReadyInternal(int new_render_process_id,
                                     int new_render_frame_id);
  void AddPendingThrottleOnIoThread(int render_process_id,
                                    int render_frame_id,
                                    IoThreadClientThrottle* pending_throttle);
  void AddExtraHeadersIfNeeded(net::URLRequest* request,
                               content::ResourceContext* resource_context);

  // Pair of render_process_id and render_frame_id.
  typedef std::pair<int, int> FrameRouteIDPair;
  typedef std::map<FrameRouteIDPair, IoThreadClientThrottle*>
      PendingThrottleMap;

  // Only accessed on the IO thread.
  PendingThrottleMap pending_throttles_;

  DISALLOW_COPY_AND_ASSIGN(AwResourceDispatcherHostDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_RENDERER_HOST_AW_RESOURCE_DISPATCHER_HOST_H_
