// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/renderer/render_frame_observer.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/guest_view/mime_handler_view_uma_types.h"
#include "extensions/common/mojo/guest_view.mojom.h"
#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
struct WebPluginInfo;
}

namespace extensions {

class MimeHandlerViewFrameContainer;

// TODO(ekaramad): Verify if we have to make this class a
// PostMessageSupport::Delegate (for full page MHV). Currently we do postMessage
// internally from ChromePrintRenderFrameHelperDelegate.
// This class is the entry point for browser issued commands related to
// MimeHandlerViews. A MHVCM helps with
// 1- Setting up beforeunload support for full page MHV
// 2- Provide postMessage support for embedded MHV.
// 3- Managing lifetime of classes and MHV state on the renderer side.
class MimeHandlerViewContainerManager
    : public content::RenderFrameObserver,
      public mojom::MimeHandlerViewContainerManager,
      public mime_handler::BeforeUnloadControl {
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

  // Called to create a MimeHandlerViewFrameContainer for an <embed> or <object>
  // element.
  bool CreateFrameContainer(const blink::WebElement& plugin_element,
                            const GURL& resource_url,
                            const std::string& mime_type,
                            const content::WebPluginInfo& plugin_info);
  // A wrapper for custom postMessage scripts. There should already be a
  // MimeHandlerViewFrameContainer for |plugin_element|.
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate);
  // Removes the |frame_container| from |frame_containers_| and destroys it. The
  // |reason| is emitted for UMA.
  void RemoveFrameContainerForReason(
      MimeHandlerViewFrameContainer* frame_container,
      MimeHandlerViewUMATypes::Type reason);
  MimeHandlerViewFrameContainer* GetFrameContainer(
      const blink::WebElement& plugin_element);
  MimeHandlerViewFrameContainer* GetFrameContainer(int32_t element_instance_id);

  // content::RenderFrameObserver.
  void OnDestruct() override;

  // mojom::MimeHandlerViewContainerManager overrides.
  void CreateBeforeUnloadControl(
      CreateBeforeUnloadControlCallback callback) override;
  void DestroyFrameContainer(int32_t element_instance_id) override;
  void DidLoad(int32_t mime_handler_view_guest_element_instance_id,
               const GURL& resource_url) override;

 private:
  bool RemoveFrameContainer(MimeHandlerViewFrameContainer* frame_container);
  // mime_handler::BeforeUnloadControl implementation.
  void SetShowBeforeUnloadDialog(
      bool show_dialog,
      SetShowBeforeUnloadDialogCallback callback) override;

  void RecordInteraction(MimeHandlerViewUMATypes::Type type);

  // Contains all the MimeHandlerViewFrameContainers under |render-frame()|.
  std::vector<std::unique_ptr<MimeHandlerViewFrameContainer>> frame_containers_;

  mojo::BindingSet<mojom::MimeHandlerViewContainerManager> bindings_;
  mojo::Binding<mime_handler::BeforeUnloadControl>
      before_unload_control_binding_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainerManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
