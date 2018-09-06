// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"

#include "base/pickle.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

namespace {
const std::string kSupportedMimeTypes[] = {"text/pdf", "application/pdf"};

bool IsSupportedMimeType(const std::string& mime_type) {
  for (const auto& type : kSupportedMimeTypes) {
    if (mime_type == type)
      return true;
  }
  return false;
}

}  // namespace

// static
bool MimeHandlerViewFrameContainer::Create(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info,
    int32_t element_instance_id) {
  if (!IsSupportedMimeType(mime_type))
    return false;
  // Life time is managed by the class itself: when the MimeHandlerViewGuest
  // is destroyed an IPC is sent to renderer to cleanup this instance.
  return new MimeHandlerViewFrameContainer(plugin_element, resource_url,
                                           mime_type, plugin_info,
                                           element_instance_id);
}

MimeHandlerViewFrameContainer::MimeHandlerViewFrameContainer(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info,
    int32_t element_instance_id)
    : MimeHandlerViewContainerBase(content::RenderFrame::FromWebFrame(
                                       plugin_element.GetDocument().GetFrame()),
                                   plugin_info,
                                   mime_type,
                                   resource_url),
      embedder_frame_(content::RenderFrame::FromWebFrame(
          plugin_element.GetDocument().GetFrame())),
      plugin_element_(plugin_element),
      element_instance_id_(element_instance_id) {
  is_embedded_ = IsEmbedded();
  if (is_embedded_) {
    SendResourceRequest();
  } else {
    // For non-embedded MimeHandlerViewGuest the stream has already been
    // intercepted.
    // TODO(ekaramad): Update |view_id_| before sending this request.
    CreateMimeHandlerViewGuestIfNecessary();
  }
}

MimeHandlerViewFrameContainer::~MimeHandlerViewFrameContainer() {}

// Returns the frame which is embedding the corresponding plugin element.
content::RenderFrame* MimeHandlerViewFrameContainer::GetEmbedderRenderFrame()
    const {
  return embedder_frame_;
}

void MimeHandlerViewFrameContainer::CreateMimeHandlerViewGuestIfNecessary() {
  if (auto* frame = GetContentFrame()) {
    plugin_frame_routing_id_ =
        content::RenderFrame::GetRoutingIdForWebFrame(frame);
  }
  if (plugin_frame_routing_id_ == MSG_ROUTING_NONE) {
    // TODO(ekaramad): Destroy and cleanup.
    return;
  }
  MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary();
}

blink::WebRemoteFrame* MimeHandlerViewFrameContainer::GetGuestProxyFrame()
    const {
  return GetContentFrame()->ToWebRemoteFrame();
}

int32_t MimeHandlerViewFrameContainer::GetInstanceId() const {
  return element_instance_id_;
}

gfx::Size MimeHandlerViewFrameContainer::GetElementSize() const {
  return gfx::Size();
}

blink::WebFrame* MimeHandlerViewFrameContainer::GetContentFrame() const {
  return blink::WebFrame::FromFrameOwnerElement(plugin_element_);
}
// mime_handler::BeforeUnloadControl implementation.
void MimeHandlerViewFrameContainer::SetShowBeforeUnloadDialog(
    bool show_dialog,
    SetShowBeforeUnloadDialogCallback callback) {
  // TODO(ekaramad): Implement.
}

bool MimeHandlerViewFrameContainer::IsEmbedded() const {
  // TODO(ekaramad): This is currently sending a request regardless of whether
  // or not this embed is due to frame navigation to resource. For such cases,
  // the renderer has already started a resource request and we should not send
  // twice. Find a way to get the intercepted stream and avoid sending an extra
  // request here.
  return true;
}

}  // namespace extensions
