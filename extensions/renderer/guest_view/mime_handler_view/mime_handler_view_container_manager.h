// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_

#include <string>

#include "extensions/common/mojo/guest_view.mojom.h"
#include "url/gurl.h"

namespace extensions {

// Helper class for omnibox and <iframe> navigations to resources that are
// handled by the MimeHandlerView (e.g., PDF).
class MimeHandlerViewContainerManager
    : public mojom::MimeHandlerViewContainerManager {
 public:
  static void BindRequest(
      int32_t routing_id,
      mojom::MimeHandlerViewContainerManagerRequest request);

  explicit MimeHandlerViewContainerManager(int32_t routing_id);
  ~MimeHandlerViewContainerManager() override;

  // mojom::MimeHandlerViewContainerManager overrides.
  void CreateFrameContainer(const GURL& resource_url,
                            const std::string& mime_type,
                            const std::string& view_id) override;

 private:
  GURL resource_url_;
  std::string mime_type_;
  std::string view_id_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainerManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
