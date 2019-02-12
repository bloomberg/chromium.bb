// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_channel_bindings.h"

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
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"

// Unique identifier of the Cast Channel message port, used by the JavaScript
// API to connect to the port.
const char kMessagePortName[] = "cast.__platform__.channel";

CastChannelBindings::CastChannelBindings(
    chromium::web::Frame* frame,
    NamedMessagePortConnector* connector,
    chromium::cast::CastChannelPtr channel_consumer,
    base::OnceClosure on_error_closure)
    : frame_(frame),
      connector_(connector),
      on_error_closure_(std::move(on_error_closure)),
      channel_consumer_(std::move(channel_consumer)) {
  DCHECK(connector_);
  DCHECK(frame_);

  channel_consumer_.set_error_handler([this](zx_status_t status) mutable {
    ZX_LOG(ERROR, status) << " Agent disconnected.";
    std::move(on_error_closure_).Run();
  });

  connector->Register(
      kMessagePortName,
      base::BindRepeating(&CastChannelBindings::OnMasterPortReceived,
                          base::Unretained(this)),
      frame_);

  // Load the cast.__platform__.channel bundle from the package assets, into a
  // mem::Buffer.
  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path));
  fuchsia::mem::Buffer bindings_buf = cr_fuchsia::MemBufferFromFile(base::File(
      assets_path.AppendASCII("fuchsia/runners/cast/cast_channel_bindings.js"),
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

CastChannelBindings::~CastChannelBindings() {
  connector_->Unregister(frame_, kMessagePortName);
}

void CastChannelBindings::OnMasterPortError() {
  master_port_.Unbind();
}

void CastChannelBindings::OnMasterPortReceived(
    chromium::web::MessagePortPtr port) {
  DCHECK(port);

  master_port_ = std::move(port);
  master_port_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(WARNING, status != ZX_ERR_PEER_CLOSED, status)
        << "Cast Channel master port disconnected.";
    OnMasterPortError();
  });
  master_port_->ReceiveMessage(fit::bind_member(
      this, &CastChannelBindings::OnCastChannelMessageReceived));
}

void CastChannelBindings::OnCastChannelMessageReceived(
    chromium::web::WebMessage message) {
  if (!message.incoming_transfer ||
      !message.incoming_transfer->is_message_port()) {
    LOG(WARNING) << "Received a CastChannel without a message port.";
    OnMasterPortError();
    return;
  }

  SendChannelToConsumer(message.incoming_transfer->message_port().Bind());

  master_port_->ReceiveMessage(fit::bind_member(
      this, &CastChannelBindings::OnCastChannelMessageReceived));
}

void CastChannelBindings::SendChannelToConsumer(
    chromium::web::MessagePortPtr channel) {
  if (consumer_ready_for_port_) {
    consumer_ready_for_port_ = false;
    channel_consumer_->OnOpened(
        std::move(channel),
        fit::bind_member(this, &CastChannelBindings::OnConsumerReadyForPort));
  } else {
    connected_channel_queue_.push_front(std::move(channel));
  }
}

void CastChannelBindings::OnConsumerReadyForPort() {
  DCHECK(!consumer_ready_for_port_);

  consumer_ready_for_port_ = true;
  if (!connected_channel_queue_.empty()) {
    // Deliver the next enqueued channel.
    chromium::web::MessagePortPtr next_port =
        std::move(connected_channel_queue_.back());
    SendChannelToConsumer(std::move(next_port));
    connected_channel_queue_.pop_back();
  }
}
