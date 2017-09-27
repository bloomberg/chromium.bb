// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_render_frame_controller_impl.h"

#include "content/public/common/isolated_world_ids.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "v8/include/v8-inspector.h"

namespace headless {

HeadlessRenderFrameControllerImpl::HeadlessRenderFrameControllerImpl(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      weak_ptr_factory_(this) {
  registry_.AddInterface(base::Bind(
      &HeadlessRenderFrameControllerImpl::OnRenderFrameControllerRequest,
      base::Unretained(this)));
}

HeadlessRenderFrameControllerImpl::~HeadlessRenderFrameControllerImpl() {}

void HeadlessRenderFrameControllerImpl::OnRenderFrameControllerRequest(
    HeadlessRenderFrameControllerRequest request) {
  headless_render_frame_controller_bindings_.AddBinding(this,
                                                        std::move(request));
}

void HeadlessRenderFrameControllerImpl::InstallTabSocket(
    int32_t execution_context_id,
    InstallTabSocketCallback callback) {
  auto find_it = tab_socket_bindings_.find(execution_context_id);
  if (find_it == tab_socket_bindings_.end()) {
    LOG(WARNING) << "InstallTabSocket failed, unknown execution_context_id "
                 << execution_context_id;
    std::move(callback).Run(false);
  } else {
    std::move(callback).Run(find_it->second.InitializeTabSocketBindings());
  }
}

void HeadlessRenderFrameControllerImpl::InstallMainWorldTabSocket(
    InstallMainWorldTabSocketCallback callback) {
  // Check any pre-existing script contexts.
  for (auto& pair : tab_socket_bindings_) {
    if (pair.second.world_id() == content::ISOLATED_WORLD_ID_GLOBAL) {
      std::move(callback).Run(
          pair.second.InitializeTabSocketBindings() ? pair.first : -1);
      return;
    }
  }
  pending_install_main_world_tab_socket_callback_ = std::move(callback);
}

void HeadlessRenderFrameControllerImpl::SendMessageToTabSocket(
    const std::string& message,
    int32_t world_id) {
  auto find_it = tab_socket_bindings_.find(world_id);
  if (find_it == tab_socket_bindings_.end()) {
    LOG(WARNING) << "Dropping message for " << world_id
                 << " because the world doesn't exist.";
    return;
  }

  find_it->second.OnMessageFromEmbedder(message);
}

void HeadlessRenderFrameControllerImpl::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void HeadlessRenderFrameControllerImpl::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  int v8_execution_context_id =
      v8_inspector::V8ContextInfo::executionContextId(context);
  auto find_it = tab_socket_bindings_.find(v8_execution_context_id);
  if (find_it != tab_socket_bindings_.end())
    tab_socket_bindings_.erase(find_it);

  auto emplace_result = tab_socket_bindings_.emplace(
      std::piecewise_construct, std::forward_as_tuple(v8_execution_context_id),
      std::forward_as_tuple(this, render_frame_, context, world_id));

  // If main world tab socket bindings have been requested and this is the main
  // world then install the bindings.
  if (world_id == content::ISOLATED_WORLD_ID_GLOBAL &&
      !pending_install_main_world_tab_socket_callback_.is_null()) {
    std::move(pending_install_main_world_tab_socket_callback_)
        .Run(emplace_result.first->second.InitializeTabSocketBindings()
                 ? v8_execution_context_id
                 : -1);
    pending_install_main_world_tab_socket_callback_ =
        InstallMainWorldTabSocketCallback();
  }
}

void HeadlessRenderFrameControllerImpl::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  tab_socket_bindings_.erase(
      v8_inspector::V8ContextInfo::executionContextId(context));
}

void HeadlessRenderFrameControllerImpl::OnDestruct() {
  delete this;
}

TabSocketPtr& HeadlessRenderFrameControllerImpl::EnsureTabSocketPtr() {
  if (!tab_socket_ptr_.is_bound()) {
    render_frame_->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&tab_socket_ptr_));
  }
  return tab_socket_ptr_;
}

}  // namespace headless
