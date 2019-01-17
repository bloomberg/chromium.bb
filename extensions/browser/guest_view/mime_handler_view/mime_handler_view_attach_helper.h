// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_ATTACH_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_ATTACH_HELPER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "content/public/browser/render_process_host_observer.h"
#include "extensions/common/mojo/guest_view.mojom.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHost;
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace extensions {

class MimeHandlerViewGuest;

// Helper class for attaching a MimeHandlerViewGuest to its outer WebContents.
// This class is exclusively used for frame-based MimeHandlerView. There is one
// instance of this class per process.
class MimeHandlerViewAttachHelper : content::RenderProcessHostObserver {
 public:
  // Returns the unique helper for process identified with |render_process_id|.
  static MimeHandlerViewAttachHelper* Get(int render_process_id);

  // During attaching guest to embedder WebContentses the corresponding plugin
  // frame might be navigated to "about:blank" first. During this time all
  // navigations for the same FrameTreeNode must be canceled.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottle(
      content::NavigationHandle* navigation_handle);

  ~MimeHandlerViewAttachHelper() override;

  // content::RenderProcessHostObserver overrides.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Starts the attaching process for the |guest_view|'s WebContents to its
  // outer WebContents (embedder WebContents) on the UI thread.
  void AttachToOuterWebContents(MimeHandlerViewGuest* guest_view,
                                int32_t embedder_render_process_id,
                                int32_t plugin_frame_routing_id,
                                int32_t element_instance_id,
                                bool is_full_page_plugin);

 private:
  class FrameNavigationHelper;
  friend class MimeHandlerViewAttachHelper::FrameNavigationHelper;

  MimeHandlerViewAttachHelper(content::RenderProcessHost* render_process_host);

  // Called by a FrameNavigationHelper on UI thread to notify the message filter
  // whether or not it should proceed with attaching a guest. If the
  // RenderFrameHost associated with |plugin_frame_routing_id| in the process
  // identified by |render_process_id_| is not found, the MimeHandlerViewGuest
  // associated with |element_instance_id| will be destroyed and deleted.
  void ResumeAttachOrDestroy(int32_t element_instance_id,
                             int32_t plugin_frame_routing_id);

  std::map<int32_t, std::unique_ptr<FrameNavigationHelper>>
      frame_navigation_helpers_;

  content::RenderProcessHost* const render_process_host_;

  base::WeakPtrFactory<MimeHandlerViewAttachHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewAttachHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_ATTACH_HELPER_H_
