// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_base.h"
#include "third_party/blink/public/web/web_element.h"

#include "url/gurl.h"

namespace blink {
class WebElement;
class WebFrame;
}  // namespace blink

namespace content {
struct WebPluginInfo;
}  // namespace content

namespace v8 {
class Isolate;
template <typename T>
class Local;
class Object;
}  // namespace v8

namespace extensions {

// The frame-based implementation of MimeHandlerViewFrameContainer. This class
// performs tasks such as requesting resource, providing postMessage API, etc.
// for an embedded MimeHandlerView extension in a cross-origin frame.
class MimeHandlerViewFrameContainer : public MimeHandlerViewContainerBase {
 public:
  static bool Create(const blink::WebElement& plugin_element,
                     const GURL& resource_url,
                     const std::string& mime_type,
                     const content::WebPluginInfo& plugin_info);
  static v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate);

  // Called by MimeHandlerViewContainerManager (calls originate from browser).
  void RetryCreatingMimeHandlerViewGuest();
  void DestroyFrameContainer();
  void DidLoad();

  int32_t element_instance_id() const { return element_instance_id_; }

 private:
  class RenderFrameLifetimeObserver;
  friend class RenderFrameLifetimeObserver;
  friend class MimeHandlerViewContainerManager;

  MimeHandlerViewFrameContainer(const blink::WebElement& plugin_element,
                                const GURL& resource_url,
                                const std::string& mime_type,
                                const content::WebPluginInfo& plugin_info);

  ~MimeHandlerViewFrameContainer() override;

  // MimeHandlerViewContainerBase overrides.
  void CreateMimeHandlerViewGuestIfNecessary() final;
  blink::WebRemoteFrame* GetGuestProxyFrame() const final;
  int32_t GetInstanceId() const final;
  gfx::Size GetElementSize() const final;

  blink::WebFrame* GetContentFrame() const;

  blink::WebElement plugin_element_;
  const int32_t element_instance_id_;
  std::unique_ptr<RenderFrameLifetimeObserver> render_frame_lifetime_observer_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewFrameContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_
