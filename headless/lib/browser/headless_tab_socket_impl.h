// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_TAB_SOCKET_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_TAB_SOCKET_IMPL_H_

#include <list>

#include "base/synchronization/lock.h"
#include "headless/lib/tab_socket.mojom.h"
#include "headless/public/headless_tab_socket.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace headless {

class HeadlessTabSocketImpl : public HeadlessTabSocket, public TabSocket {
 public:
  HeadlessTabSocketImpl();
  ~HeadlessTabSocketImpl() override;

  // HeadlessTabSocket implementation:
  void SendMessageToTab(const std::string& message) override;
  void SetListener(Listener* listener) override;

  // TabSocket implementation:
  void SendMessageToEmbedder(const std::string& message) override;
  void AwaitNextMessageFromEmbedder(
      AwaitNextMessageFromEmbedderCallback callback) override;

  void CreateMojoService(mojo::InterfaceRequest<TabSocket> request);

 private:
  base::Lock lock_;  // Protects everything below.
  AwaitNextMessageFromEmbedderCallback waiting_for_message_cb_;
  std::list<std::string> outgoing_message_queue_;
  std::list<std::string> incoming_message_queue_;
  Listener* listener_;  // NOT OWNED

  // Must be listed last so it gets destructed before |waiting_for_message_cb_|.
  mojo::BindingSet<TabSocket> mojo_bindings_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessTabSocketImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_TAB_SOCKET_IMPL_H_
