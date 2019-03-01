// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_

#include <string>

#include "content/public/renderer/render_frame_observer.h"
#include "extensions/common/mojo/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}

namespace extensions {

class MimeHandlerViewFrameContainer;

// The entry point for browser issued commands related to MimeHandlerView. This
// class essentially helps tasks such as frame navigations to MimeHandlerView
// mime-types (e.g., PDF) and in general creating and destroying the renderer
// state.
class MimeHandlerViewContainerManager
    : public content::RenderFrameObserver,
      public mojom::MimeHandlerViewContainerManager {
 public:
  static void BindRequest(
      int32_t routing_id,
      mojom::MimeHandlerViewContainerManagerRequest request);
  // Returns the container manager associated with |render_frame|. If none
  // exists and |create_if_does_not_exist| is set true, creates and returns a
  // new instance for |render_frame|.
  static MimeHandlerViewContainerManager* Get(
      content::RenderFrame* render_frame,
      bool create_if_does_not_exist = false);

  explicit MimeHandlerViewContainerManager(content::RenderFrame* render_frame);
  ~MimeHandlerViewContainerManager() override;

  // content::RenderFrameObserver.
  void OnDestruct() override;

  // mojom::MimeHandlerViewContainerManager overrides.
  void CreateFrameContainer(const GURL& resource_url,
                            const std::string& mime_type,
                            const std::string& view_id) override;
  void DestroyFrameContainer(int32_t element_instance_id) override;
  void RetryCreatingMimeHandlerViewGuest(int32_t element_instance_id) override;
  void DidLoad(int32_t element_instance_id) override;

 private:
  MimeHandlerViewFrameContainer* GetFrameContainer(int32_t instance_id);

  mojo::BindingSet<mojom::MimeHandlerViewContainerManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainerManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
