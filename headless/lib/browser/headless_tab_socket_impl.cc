// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_tab_socket_impl.h"

#include "base/stl_util.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace headless {

HeadlessTabSocketImpl::HeadlessTabSocketImpl(content::WebContents* web_contents)
    : web_contents_(web_contents),
      listener_(nullptr),
      weak_ptr_factory_(this) {}

HeadlessTabSocketImpl::~HeadlessTabSocketImpl() = default;

// Wrangles the async responses to
// HeadlessRenderFrameControllerImpl::InstallTabSocket for which at most one
// should succeed.
class TabSocketInstallationController
    : public base::RefCounted<TabSocketInstallationController> {
 public:
  TabSocketInstallationController(
      int v8_execution_context_id,
      size_t render_frame_count,
      base::WeakPtr<HeadlessTabSocketImpl> headless_tab_socket_impl,
      base::Callback<void(bool)> callback)
      : v8_execution_context_id_(v8_execution_context_id),
        render_frame_count_(render_frame_count),
        headless_tab_socket_impl_(headless_tab_socket_impl),
        callback_(callback),
        success_(false) {}

  void InstallTabSocketCallback(content::RenderFrameHost* render_frame_host,
                                bool success) {
    render_frame_count_--;

    // It's possible the HeadlessTabSocketImpl went away, if that happened we
    // don't want to pretend TabSocket installation succeeded.
    if (!headless_tab_socket_impl_)
      success = false;

    if (success) {
      CHECK(!success_) << "At most one InstallTabSocket call should succeed!";
      success_ = true;
      headless_tab_socket_impl_->v8_execution_context_id_to_render_frame_host_
          .insert(std::make_pair(v8_execution_context_id_, render_frame_host));

      callback_.Run(true);
    } else if (render_frame_count_ == 0 && !success_) {
      callback_.Run(false);
    }
  }

 private:
  int v8_execution_context_id_;
  size_t render_frame_count_;

  base::WeakPtr<HeadlessTabSocketImpl> headless_tab_socket_impl_;
  base::Callback<void(bool)> callback_;
  bool success_;

  friend class base::RefCounted<TabSocketInstallationController>;
  ~TabSocketInstallationController() = default;
};

void HeadlessTabSocketImpl::InstallHeadlessTabSocketBindings(
    int v8_execution_context_id,
    base::Callback<void(bool)> callback) {
  // We need to find the right RenderFrameHost to install the bindings on but
  // the browser doesn't know which RenderFrameHost |v8_execution_context_id|
  // corresponds to if any. So we try all of them.
  scoped_refptr<TabSocketInstallationController>
      tab_socket_installation_controller = new TabSocketInstallationController(
          v8_execution_context_id, render_frame_hosts_.size(),
          weak_ptr_factory_.GetWeakPtr(), callback);
  for (content::RenderFrameHost* render_frame_host : render_frame_hosts_) {
    HeadlessRenderFrameControllerPtr& headless_render_frame_controller =
        render_frame_controllers_[render_frame_host];
    if (!headless_render_frame_controller.is_bound()) {
      render_frame_host->GetRemoteInterfaces()->GetInterface(
          &headless_render_frame_controller);
    }

    // This will only succeed if the |render_frame_host_controller| contains
    // |v8_execution_context_id|. The TabSocketInstallationController keeps
    // track of how many callbacks have been received and if all of them have
    // been unsuccessful it runs |callback| with false.  If one of them succeeds
    //  it runs |callback| with true.
    headless_render_frame_controller->InstallTabSocket(
        v8_execution_context_id,
        base::Bind(&TabSocketInstallationController::InstallTabSocketCallback,
                   tab_socket_installation_controller, render_frame_host));
  }
}

void HeadlessTabSocketImpl::InstallMainFrameMainWorldHeadlessTabSocketBindings(
    base::Callback<void(base::Optional<int>)> callback) {
  content::RenderFrameHost* main_frame = web_contents_->GetMainFrame();
  HeadlessRenderFrameControllerPtr& headless_render_frame_controller =
      render_frame_controllers_[main_frame];
  if (!headless_render_frame_controller.is_bound()) {
    main_frame->GetRemoteInterfaces()->GetInterface(
        &headless_render_frame_controller);
  }
  headless_render_frame_controller->InstallMainWorldTabSocket(
      base::Bind(&HeadlessTabSocketImpl::OnInstallMainWorldTabSocket,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void HeadlessTabSocketImpl::OnInstallMainWorldTabSocket(
    base::Callback<void(base::Optional<int>)> callback,
    int v8_execution_context_id) {
  if (v8_execution_context_id == -1) {
    callback.Run(base::nullopt);
  } else {
    v8_execution_context_id_to_render_frame_host_.insert(
        std::make_pair(v8_execution_context_id, web_contents_->GetMainFrame()));
    callback.Run(v8_execution_context_id);
  }
}

void HeadlessTabSocketImpl::SendMessageToContext(
    const std::string& message,
    int32_t v8_execution_context_id) {
  auto render_frame_host = v8_execution_context_id_to_render_frame_host_.find(
      v8_execution_context_id);
  if (render_frame_host ==
      v8_execution_context_id_to_render_frame_host_.end()) {
    LOG(WARNING) << "Unknown v8_execution_context_id "
                 << v8_execution_context_id;
    return;
  }

  auto render_frame_controller =
      render_frame_controllers_.find(render_frame_host->second);
  if (render_frame_controller == render_frame_controllers_.end()) {
    LOG(WARNING) << "Unknown RenderFrameHist " << render_frame_host->second;
    return;
  }
  render_frame_controller->second->SendMessageToTabSocket(
      message, v8_execution_context_id);
}

void HeadlessTabSocketImpl::SetListener(Listener* listener) {
  MessageQueue messages;

  {
    base::AutoLock lock(lock_);
    listener_ = listener;
    if (!listener)
      return;

    std::swap(messages, from_tab_message_queue_);
  }

  for (const Message& message : messages) {
    listener_->OnMessageFromContext(message.first, message.second);
  }
}

void HeadlessTabSocketImpl::SendMessageToEmbedder(
    const std::string& message,
    int32_t v8_execution_context_id) {
  Listener* listener = nullptr;
  {
    base::AutoLock lock(lock_);
    CHECK(v8_execution_context_id_to_render_frame_host_.find(
              v8_execution_context_id) !=
          v8_execution_context_id_to_render_frame_host_.end())
        << "Unknown v8_execution_context_id " << v8_execution_context_id;
    if (listener_) {
      listener = listener_;
    } else {
      from_tab_message_queue_.emplace_back(message, v8_execution_context_id);
      return;
    }
  }

  listener->OnMessageFromContext(message, v8_execution_context_id);
}

void HeadlessTabSocketImpl::CreateMojoService(
    mojo::InterfaceRequest<TabSocket> request) {
  mojo_bindings_.AddBinding(this, std::move(request));
}

void HeadlessTabSocketImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  render_frame_hosts_.insert(render_frame_host);
}

void HeadlessTabSocketImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // Remove all entries from |v8_execution_context_id_to_render_frame_host_|
  // where the mapped value is |render_frame_host|.
  base::EraseIf(v8_execution_context_id_to_render_frame_host_,
                [render_frame_host](
                    const std::pair<int, content::RenderFrameHost*>& pair) {
                  return render_frame_host == pair.second;
                });

  render_frame_controllers_.erase(render_frame_host);
  render_frame_hosts_.erase(render_frame_host);
}

}  // namespace headless
