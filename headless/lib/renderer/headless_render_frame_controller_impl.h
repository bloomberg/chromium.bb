// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_RENDERER_HEADLESS_RENDER_FRAME_CONTROLLER_IMPL_H_
#define HEADLESS_LIB_RENDERER_HEADLESS_RENDER_FRAME_CONTROLLER_IMPL_H_

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "headless/lib/headless_render_frame_controller.mojom.h"
#include "headless/lib/renderer/headless_tab_socket_bindings.h"
#include "headless/lib/tab_socket.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace headless {

class HeadlessRenderFrameControllerImpl : public HeadlessRenderFrameController,
                                          public content::RenderFrameObserver {
 public:
  explicit HeadlessRenderFrameControllerImpl(
      content::RenderFrame* render_frame);
  ~HeadlessRenderFrameControllerImpl() override;

  void OnRenderFrameControllerRequest(
      HeadlessRenderFrameControllerRequest request);

  // HeadlessRenderFrameController implementation:
  void InstallTabSocket(int32_t v8_execution_context_id,
                        InstallTabSocketCallback callback) override;
  void InstallMainWorldTabSocket(
      InstallMainWorldTabSocketCallback callback) override;
  void SendMessageToTabSocket(const std::string& message,
                              int32_t world_id) override;

  // content::RenderFrameObserver implementation:
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int world_id) override;

  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override;

  void OnDestruct() override;

  TabSocketPtr& EnsureTabSocketPtr();

 private:
  content::RenderFrame* const render_frame_;  // NOT OWNED
  mojo::BindingSet<HeadlessRenderFrameController>
      headless_render_frame_controller_bindings_;
  std::map<int, HeadlessTabSocketBindings> tab_socket_bindings_;
  TabSocketPtr tab_socket_ptr_;
  InstallMainWorldTabSocketCallback
      pending_install_main_world_tab_socket_callback_;
  service_manager::BinderRegistry registry_;
  base::WeakPtrFactory<HeadlessRenderFrameControllerImpl> weak_ptr_factory_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_RENDERER_HEADLESS_RENDER_FRAME_CONTROLLER_IMPL_H_
