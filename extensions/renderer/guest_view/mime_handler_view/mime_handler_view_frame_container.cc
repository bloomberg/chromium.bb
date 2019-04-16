// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"

#include <string>

#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

class MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver
    : public content::RenderFrameObserver {
 public:
  RenderFrameLifetimeObserver(MimeHandlerViewFrameContainer* container,
                              content::RenderFrame* render_frame);
  ~RenderFrameLifetimeObserver() override;

  // content:RenderFrameObserver override.
  void OnDestruct() final;

 private:
  MimeHandlerViewFrameContainer* const container_;
};

MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver::
    RenderFrameLifetimeObserver(MimeHandlerViewFrameContainer* container,
                                content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame), container_(container) {}

MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver::
    ~RenderFrameLifetimeObserver() {}

void MimeHandlerViewFrameContainer::RenderFrameLifetimeObserver::OnDestruct() {
  container_->DestroyFrameContainer();
}

// static
bool MimeHandlerViewFrameContainer::Create(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info) {
  if (plugin_info.type != content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN) {
    // TODO(ekaramad): Rename this plugin type once https://crbug.com/659750 is
    // fixed. We only create a MHVFC for the plugin types of BrowserPlugin
    // (which used to create a MimeHandlerViewContainer).
    return false;
  }

  // Life time is managed by the class itself: when the MimeHandlerViewGuest
  // is destroyed an IPC is sent to renderer to cleanup this instance.
  return new MimeHandlerViewFrameContainer(plugin_element, resource_url,
                                           mime_type, plugin_info);
}

v8::Local<v8::Object> MimeHandlerViewFrameContainer::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
  auto containers = FromRenderFrame(content::RenderFrame::FromWebFrame(
      plugin_element.GetDocument().GetFrame()));
  for (auto* container : containers) {
    auto* frame_container =
        static_cast<MimeHandlerViewFrameContainer*>(container);
    if (frame_container->plugin_element_ != plugin_element)
      continue;
    return frame_container->GetScriptableObjectInternal(isolate);
  }
  return v8::Local<v8::Object>();
}

MimeHandlerViewFrameContainer::MimeHandlerViewFrameContainer(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info)
    : MimeHandlerViewContainerBase(content::RenderFrame::FromWebFrame(
                                       plugin_element.GetDocument().GetFrame()),
                                   plugin_info,
                                   mime_type,
                                   resource_url),
      plugin_element_(plugin_element),
      element_instance_id_(content::RenderThread::Get()->GenerateRoutingID()),
      render_frame_lifetime_observer_(
          new RenderFrameLifetimeObserver(this, GetEmbedderRenderFrame())) {
  RecordInteraction(
      MimeHandlerViewUMATypes::Type::kDidCreateMimeHandlerViewContainerBase);
  is_embedded_ = true;
  SendResourceRequest();
}

MimeHandlerViewFrameContainer::~MimeHandlerViewFrameContainer() {}

void MimeHandlerViewFrameContainer::CreateMimeHandlerViewGuestIfNecessary() {
  if (auto* frame = GetContentFrame()) {
    plugin_frame_routing_id_ =
        content::RenderFrame::GetRoutingIdForWebFrame(frame);
  }
  if (plugin_frame_routing_id_ == MSG_ROUTING_NONE) {
    DestroyFrameContainer();
    return;
  }
  MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewFrameContainer::RetryCreatingMimeHandlerViewGuest() {
  CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewFrameContainer::DidLoad() {
  DidLoadInternal();
}

void MimeHandlerViewFrameContainer::DestroyFrameContainer() {
  delete this;
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
  DCHECK(is_embedded_);
  return blink::WebFrame::FromFrameOwnerElement(plugin_element_);
}

}  // namespace extensions
