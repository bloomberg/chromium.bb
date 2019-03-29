// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_embedder.h"

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/mojo/guest_view.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace extensions {

namespace {
using EmbedderMap =
    base::flat_map<int32_t, std::unique_ptr<MimeHandlerViewEmbedder>>;

EmbedderMap* GetMimeHandlerViewEmbeddersMap() {
  static base::NoDestructor<EmbedderMap> instance;
  return instance.get();
}
}  // namespace

void MimeHandlerViewEmbedder::Create(int32_t frame_tree_node_id,
                                     const GURL& resource_url,
                                     const std::string& mime_type,
                                     const std::string& stream_id) {
  DCHECK(!base::ContainsKey(*GetMimeHandlerViewEmbeddersMap(),
                            frame_tree_node_id));
  GetMimeHandlerViewEmbeddersMap()->insert_or_assign(
      frame_tree_node_id,
      base::WrapUnique(new MimeHandlerViewEmbedder(
          frame_tree_node_id, resource_url, mime_type, stream_id)));
}

MimeHandlerViewEmbedder::MimeHandlerViewEmbedder(int32_t frame_tree_node_id,
                                                 const GURL& resource_url,
                                                 const std::string& mime_type,
                                                 const std::string& stream_id)
    : content::WebContentsObserver(
          content::WebContents::FromFrameTreeNodeId(frame_tree_node_id)),
      frame_tree_node_id_(frame_tree_node_id),
      resource_url_(resource_url),
      mime_type_(mime_type),
      stream_id_(stream_id) {}

MimeHandlerViewEmbedder::~MimeHandlerViewEmbedder() {}

void MimeHandlerViewEmbedder::DidStartNavigation(
    content::NavigationHandle* handle) {
  // This observer is created after the observed |frame_tree_node_id_| started
  // its navigation to the |resource_url|. If any new navigations start then
  // we should stop now and do not create a MHVG.
  if (handle->GetFrameTreeNodeId() == frame_tree_node_id_)
    GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
}

void MimeHandlerViewEmbedder::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParent() &&
      render_frame_host->GetParent()->GetFrameTreeNodeId() ==
          frame_tree_node_id_ &&
      render_frame_host->GetParent()->GetLastCommittedURL() == resource_url_) {
    // This suggests that a same-origin child frame is created under the
    // RFH associated with |frame_tree_node_id_|. This suggests that the HTML
    // string is loaded in the observed frame's document and now the renderer
    // can initiate the MimeHandlerViewFrameContainer creation process.
    mojom::MimeHandlerViewContainerManagerPtr container_manager;
    render_frame_host->GetParent()->GetRemoteInterfaces()->GetInterface(
        &container_manager);
    container_manager->CreateFrameContainer(resource_url_, mime_type_,
                                            stream_id_);
    GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
  }
}

void MimeHandlerViewEmbedder::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetFrameTreeNodeId() == frame_tree_node_id_)
    GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
}

}  // namespace extensions
