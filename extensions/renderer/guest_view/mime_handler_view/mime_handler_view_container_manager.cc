// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"

#include <memory>
#include <utility>

#include "content/public/renderer/render_frame.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/url_constants.h"

namespace extensions {

// static
void MimeHandlerViewContainerManager::BindRequest(
    int32_t routing_id,
    mojom::MimeHandlerViewContainerManagerRequest request) {
  auto* render_frame = content::RenderFrame::FromRoutingID(routing_id);
  if (!render_frame)
    return;
  mojo::MakeStrongBinding(
      std::make_unique<MimeHandlerViewContainerManager>(routing_id),
      std::move(request));
}

MimeHandlerViewContainerManager::MimeHandlerViewContainerManager(
    int32_t routing_id) {}

MimeHandlerViewContainerManager::~MimeHandlerViewContainerManager() {}

void MimeHandlerViewContainerManager::CreateFrameContainer(
    const GURL& resource_url,
    const std::string& mime_type,
    const std::string& view_id) {
  // TODO(ekaramad): Implement (https://crbug.com/659750).
}

}  // namespace extensions
