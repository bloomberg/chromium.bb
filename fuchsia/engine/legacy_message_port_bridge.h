// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_LEGACY_MESSAGE_PORT_BRIDGE_H_
#define FUCHSIA_ENGINE_LEGACY_MESSAGE_PORT_BRIDGE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/optional.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

namespace cr_fuchsia {

// Allows chromium::web::MessagePort clients to connect to
// fuchsia::web::MessagePort instances.
//
// LegacyMessagePortBridge are self-managed; they destroy themselves when
// the connection with either end is terminated.
class LegacyMessagePortBridge : public chromium::web::MessagePort {
 public:
  explicit LegacyMessagePortBridge(
      fidl::InterfaceRequest<chromium::web::MessagePort> request,
      fuchsia::web::MessagePortPtr handle);

  static base::Optional<fuchsia::web::WebMessage> ConvertFromLegacyWebMessage(
      chromium::web::WebMessage& message);

 private:
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

}  // namespace cr_fuchsia

#endif  // FUCHSIA_ENGINE_LEGACY_MESSAGE_PORT_BRIDGE_H_
