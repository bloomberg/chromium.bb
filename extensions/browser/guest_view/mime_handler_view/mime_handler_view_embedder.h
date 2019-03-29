// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_

#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace extensions {

// MimeHandlerViewEmbedder is instantiated in response to a frame navigation to
// a resource with a MIME type handled by MimeHandlerViewGuest. The MHVE class
// handles tasks related to navigation in the MHVG's embedder frame. It manages
// its own lifetime and destroys itself if:
//  a- Navigation to the resource is interrupted by a new navigation or the
//     frame is destroyed.
//  b- Document successfully loads and the MimeHandlerViewFrameContainer is
//     created in the renderer.
class MimeHandlerViewEmbedder : public content::WebContentsObserver {
 public:
  static void Create(int32_t frame_tree_node_id,
                     const GURL& resource_url,
                     const std::string& mime_type,
                     const std::string& stream_id);

  ~MimeHandlerViewEmbedder() override;

  // content::WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void FrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;

 private:
  MimeHandlerViewEmbedder(int32_t frame_tree_node_id,
                          const GURL& resource_url,
                          const std::string& mime_type,
                          const std::string& stream_id);

  int32_t frame_tree_node_id_;
  const GURL resource_url_;
  const std::string mime_type_;
  const std::string stream_id_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewEmbedder);
};

}  // namespace extensions
#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_
