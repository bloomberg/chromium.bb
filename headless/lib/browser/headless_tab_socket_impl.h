// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_TAB_SOCKET_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_TAB_SOCKET_IMPL_H_

#include <list>

#include "base/synchronization/lock.h"
#include "headless/lib/headless_render_frame_controller.mojom.h"
#include "headless/lib/tab_socket.mojom.h"
#include "headless/public/headless_tab_socket.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace headless {

class HeadlessTabSocketImpl : public HeadlessTabSocket, public TabSocket {
 public:
  explicit HeadlessTabSocketImpl(content::WebContents* web_contents);
  ~HeadlessTabSocketImpl() override;

  // HeadlessTabSocket implementation:
  void InstallHeadlessTabSocketBindings(
      int v8_execution_context_id,
      base::Callback<void(bool)> callback) override;
  void InstallMainFrameMainWorldHeadlessTabSocketBindings(
      base::Callback<void(base::Optional<int>)> callback) override;
  void SendMessageToContext(const std::string& message,
                            int v8_execution_context_id) override;
  void SetListener(Listener* listener) override;

  // TabSocket implementation:
  void SendMessageToEmbedder(const std::string& message,
                             int32_t v8_execution_context_id) override;

  void CreateMojoService(mojo::InterfaceRequest<TabSocket> request);

  void RenderFrameCreated(content::RenderFrameHost* render_frame_host);
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host);

 private:
  friend class TabSocketInstallationController;

  void OnInstallMainWorldTabSocket(
      base::Callback<void(base::Optional<int>)> callback,
      int world_id);

  base::Lock lock_;  // Protects everything below.
  using Message = std::pair<std::string, int>;
  using MessageQueue = std::list<Message>;

  MessageQueue from_tab_message_queue_;
  content::WebContents* web_contents_;  // NOT OWNED
  Listener* listener_;  // NOT OWNED

  mojo::BindingSet<TabSocket> mojo_bindings_;

  std::set<content::RenderFrameHost*> render_frame_hosts_;
  std::map<int, content::RenderFrameHost*>
      v8_execution_context_id_to_render_frame_host_;
  std::map<content::RenderFrameHost*, HeadlessRenderFrameControllerPtr>
      render_frame_controllers_;
  base::WeakPtrFactory<HeadlessTabSocketImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessTabSocketImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_TAB_SOCKET_IMPL_H_
