// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/bindings/cast_channel.h"

#include <lib/fit/function.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/threading/thread_task_runner_handle.h"
#include "fuchsia/common/mem_buffer_util.h"
#include "fuchsia/common/named_message_port_connector.h"

// Unique identifier of the Cast Channel message port, used by the JavaScript
// API to connect to the port.
const char kMessagePortName[] = "cast.__platform__.channel";

CastChannelImpl::CastChannelImpl(
    chromium::web::Frame* frame,
    webrunner::NamedMessagePortConnector* connector)
    : frame_(frame), connector_(connector) {
  DCHECK(connector_);
  DCHECK(frame_);

  connector->Register(
      kMessagePortName,
      base::BindRepeating(&CastChannelImpl::OnMasterPortReceived,
                          base::Unretained(this)),
      frame_);

  // Load the cast.__platform__.channel bundle from the package assets, into a
  // mem::Buffer.
  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path));
  fuchsia::mem::Buffer bindings_buf = webrunner::MemBufferFromFile(base::File(
      assets_path.AppendASCII("fuchsia/runners/cast/bindings/cast_channel.js"),
      base::File::FLAG_OPEN | base::File::FLAG_READ));
  CHECK(bindings_buf.vmo);

  // Inject the cast.__platform__.channel API to all origins.
  std::vector<std::string> origins = {"*"};

  // Configure the bundle to be re-injected every time the |frame_| content is
  // loaded.
  frame_->ExecuteJavaScript(
      std::move(origins), std::move(bindings_buf),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { CHECK(success) << "Couldn't insert bindings."; });
}

CastChannelImpl::~CastChannelImpl() {
  connector_->Unregister(frame_, kMessagePortName);
}

void CastChannelImpl::OnMasterPortError() {
  master_port_.Unbind();
}

void CastChannelImpl::Connect(ConnectCallback callback) {
  // If there is already a bound Cast Channel ready, then return it.
  if (pending_channel_) {
    callback(std::move(pending_channel_));
    return;
  }

  pending_connect_cb_ = std::move(callback);

  if (master_port_) {
    master_port_->ReceiveMessage(
        fit::bind_member(this, &CastChannelImpl::OnCastChannelMessageReceived));
  }

  // If there is no master port available at this time, then defer invocation of
  // |pending_connect_cb_| until the master port has been received.
}

void CastChannelImpl::OnMasterPortReceived(chromium::web::MessagePortPtr port) {
  DCHECK(port);

  master_port_ = std::move(port);
  master_port_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(WARNING, status != ZX_ERR_PEER_CLOSED, status)
        << "Cast Channel master port disconnected.";
    OnMasterPortError();
  });

  if (pending_connect_cb_) {
    // Resolve the in-flight Connect call.
    Connect(std::move(pending_connect_cb_));
  }
}

void CastChannelImpl::OnCastChannelMessageReceived(
    chromium::web::WebMessage message) {
  if (!message.incoming_transfer ||
      !message.incoming_transfer->is_message_port()) {
    LOG(WARNING) << "Received a CastChannel without a message port.";
    OnMasterPortError();
    return;
  }

  // Fulfill an outstanding Connect() operation, if there is one.
  if (pending_connect_cb_) {
    pending_connect_cb_(std::move(message.incoming_transfer->message_port()));
    pending_connect_cb_ = {};
    return;
  }

  pending_channel_ = std::move(message.incoming_transfer->message_port());
}
