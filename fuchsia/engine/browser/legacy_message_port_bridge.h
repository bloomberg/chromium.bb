// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_LEGACY_MESSAGE_PORT_BRIDGE_H_
#define FUCHSIA_ENGINE_BROWSER_LEGACY_MESSAGE_PORT_BRIDGE_H_

#include <lib/fidl/cpp/binding.h>

#include "fuchsia/engine/browser/message_port_impl.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

// Bridges chromium::web::MessagePort and fuchsia::web::MessagePort calls.
//
// LegacyMessagePortBridge are self-managed; they destroy themselves when
// the connection with either the chromium::web::MessagePort or
// fuchsia::web::MessagePort is terminated.
class LegacyMessagePortBridge : public chromium::web::MessagePort {
 public:
  // Creates a connected MessagePort from a FIDL MessagePort request and
  // returns a handle to its peer Mojo pipe.
  static mojo::ScopedMessagePipeHandle FromFidl(
      fidl::InterfaceRequest<chromium::web::MessagePort> port);

 private:
  explicit LegacyMessagePortBridge(
      fidl::InterfaceRequest<chromium::web::MessagePort> request,
      fuchsia::web::MessagePortPtr handle);

  // Non-public to ensure that only this object may destroy itself.
  ~LegacyMessagePortBridge() override;

  // chromium::web::MessagePort implementation.
  void PostMessage(chromium::web::WebMessage message,
                   PostMessageCallback callback) override;
  void ReceiveMessage(ReceiveMessageCallback callback) override;

  fidl::Binding<chromium::web::MessagePort> binding_;
  fuchsia::web::MessagePortPtr message_port_;

  DISALLOW_COPY_AND_ASSIGN(LegacyMessagePortBridge);
};

#endif  // FUCHSIA_ENGINE_BROWSER_LEGACY_MESSAGE_PORT_BRIDGE_H_
