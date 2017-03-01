// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_

#include <stdint.h>

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class ResourceDispatcherHostImpl;
class SessionStorageNamespace;

// Instantiated per RenderProcessHost to provide various optimizations on
// behalf of a RenderWidgetHost.  This class bridges between the IO thread
// where the RenderProcessHost's MessageFilter lives and the UI thread where
// the RenderWidgetHost lives.
class RenderWidgetHelper
    : public base::RefCountedThreadSafe<RenderWidgetHelper,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  RenderWidgetHelper();

  void Init(int render_process_id,
            ResourceDispatcherHostImpl* resource_dispatcher_host);

  // Gets the next available routing id.  This is thread safe.
  int GetNextRoutingID();

  // IO THREAD ONLY -----------------------------------------------------------

  // Lookup the RenderWidgetHelper from the render_process_host_id. Returns NULL
  // if not found. NOTE: The raw pointer is for temporary use only. To retain,
  // store in a scoped_refptr.
  static RenderWidgetHelper* FromProcessHostID(int render_process_host_id);

  // UI THREAD ONLY -----------------------------------------------------------

  // These two functions provide the backend implementation of the
  // corresponding functions in RenderProcessHost. See those declarations
  // for documentation.
  void ResumeDeferredNavigation(const GlobalRequestID& request_id);

  // IO THREAD ONLY -----------------------------------------------------------

  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       bool no_javascript_access,
                       int32_t* render_view_route_id,
                       int32_t* main_frame_route_id,
                       int32_t* main_frame_widget_route_id,
                       SessionStorageNamespace* session_storage_namespace);
  void CreateNewWidget(int opener_id,
                       blink::WebPopupType popup_type,
                       int* route_id);
  void CreateNewFullscreenWidget(int opener_id, int* route_id);

 private:
  friend class base::RefCountedThreadSafe<RenderWidgetHelper>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<RenderWidgetHelper>;

  ~RenderWidgetHelper();

  // Called on the UI thread to finish creating a window.
  void OnCreateNewWindowOnUI(
      mojom::CreateNewWindowParamsPtr params,
      int32_t render_view_route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      SessionStorageNamespace* session_storage_namespace);

  // Called on the UI thread to finish creating a widget.
  void OnCreateWidgetOnUI(int32_t opener_id,
                          int32_t route_id,
                          blink::WebPopupType popup_type);

  // Called on the UI thread to create a fullscreen widget.
  void OnCreateFullscreenWidgetOnUI(int32_t opener_id, int32_t route_id);

  // Called on the IO thread to resume a paused navigation in the network
  // stack without transferring it to a new renderer process.
  void OnResumeDeferredNavigation(const GlobalRequestID& request_id);

  // Called on the IO thread to resume a navigation paused immediately after
  // receiving response headers.
  void OnResumeResponseDeferredAtStart(const GlobalRequestID& request_id);

  int render_process_id_;

  // The next routing id to use.
  base::AtomicSequenceNumber next_routing_id_;

  ResourceDispatcherHostImpl* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_
