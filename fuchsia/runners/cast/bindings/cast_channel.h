// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_BINDINGS_CAST_CHANNEL_H_
#define FUCHSIA_RUNNERS_CAST_BINDINGS_CAST_CHANNEL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "fuchsia/common/fuchsia_export.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

namespace webrunner {
class NamedMessagePortConnector;
}

// Handles the injection of cast.__platform__.channel bindings into pages'
// scripting context, and establishes a bidirectional message pipe over
// which the two communicate.
class FUCHSIA_EXPORT CastChannelImpl : public chromium::cast::CastChannel {
 public:
  // Attaches CastChannel bindings and port to a |frame|.
  // |frame|: The frame to be provided with a CastChannel.
  // |connector|: The NamedMessagePortConnector to use for establishing
  // transport.
  // Both |frame| and |connector| must outlive |this|.
  CastChannelImpl(chromium::web::Frame* frame,
                  webrunner::NamedMessagePortConnector* connector);
  ~CastChannelImpl() override;

  // chromium::cast::CastChannel implementation.
  void Connect(ConnectCallback callback) override;

 private:
  // Receives a port used for receiving new Cast Channel ports.
  void OnMasterPortReceived(chromium::web::MessagePortPtr port);

  // Receives a message containing a newly opened Cast Channel from
  // |master_port_|.
  void OnCastChannelMessageReceived(chromium::web::WebMessage message);

  // Handles error conditions on |master_port_|.
  void OnMasterPortError();

  chromium::web::Frame* const frame_;
  webrunner::NamedMessagePortConnector* const connector_;

  // A long-lived port, used to receive new Cast Channel ports when they are
  // opened. Should be automatically  populated by the
  // NamedMessagePortConnector whenever |frame| loads a new page.
  chromium::web::MessagePortPtr master_port_;

  fuchsia::mem::Buffer bindings_script_;
  ConnectCallback pending_connect_cb_;

  // A Cast Channel received from the webpage, waiting to be handled via
  // ListenForChannel().
  fidl::InterfaceHandle<chromium::web::MessagePort> pending_channel_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelImpl);
};

#endif  // FUCHSIA_RUNNERS_CAST_BINDINGS_CAST_CHANNEL_H_
