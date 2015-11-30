// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/browser_connection_handler.h"

#include "base/logging.h"
#include "base/macros.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_demultiplexer.h"
#include "blimp/net/blimp_message_multiplexer.h"
#include "blimp/net/blimp_message_output_buffer.h"
#include "blimp/net/blimp_message_processor.h"

namespace blimp {

BrowserConnectionHandler::BrowserConnectionHandler()
    : demultiplexer_(new BlimpMessageDemultiplexer),
      output_buffer_(new BlimpMessageOutputBuffer),
      multiplexer_(new BlimpMessageMultiplexer(output_buffer_.get())) {}

BrowserConnectionHandler::~BrowserConnectionHandler() {}

scoped_ptr<BlimpMessageProcessor> BrowserConnectionHandler::RegisterFeature(
    BlimpMessage::Type type,
    BlimpMessageProcessor* incoming_processor) {
  demultiplexer_->AddProcessor(type, incoming_processor);
  return multiplexer_->CreateSenderForType(type);
}

void BrowserConnectionHandler::HandleConnection(
    scoped_ptr<BlimpConnection> connection) {
  // Since there is only a single Client, assume a newer connection should
  // replace an existing one.
  DropCurrentConnection();
  connection_ = connection.Pass();

  // Connect the incoming & outgoing message streams.
  connection_->SetIncomingMessageProcessor(demultiplexer_.get());
  output_buffer_->set_output_processor(
      connection_->GetOutgoingMessageProcessor());
}

void BrowserConnectionHandler::DropCurrentConnection() {
  if (!connection_)
    return;
  connection_->SetIncomingMessageProcessor(nullptr);
  output_buffer_->set_output_processor(nullptr);
  connection_.reset();
}

void BrowserConnectionHandler::OnConnectionError(int error) {
  LOG(WARNING) << "Connection error " << error;
  DropCurrentConnection();
}

}  // namespace blimp
