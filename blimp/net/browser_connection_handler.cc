// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/browser_connection_handler.h"

#include "base/logging.h"
#include "base/macros.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_checkpointer.h"
#include "blimp/net/blimp_message_demultiplexer.h"
#include "blimp/net/blimp_message_multiplexer.h"
#include "blimp/net/blimp_message_output_buffer.h"
#include "blimp/net/blimp_message_processor.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace {

// Maximum footprint of the output buffer.
// TODO(kmarshall): Use a value that's computed from the platform.
const int kMaxBufferSizeBytes = 1 << 24;

}  // namespace

BrowserConnectionHandler::BrowserConnectionHandler()
    : demultiplexer_(new BlimpMessageDemultiplexer),
      output_buffer_(new BlimpMessageOutputBuffer(kMaxBufferSizeBytes)),
      multiplexer_(new BlimpMessageMultiplexer(output_buffer_.get())),
      checkpointer_(new BlimpMessageCheckpointer(demultiplexer_.get(),
                                                 output_buffer_.get(),
                                                 output_buffer_.get())) {}

BrowserConnectionHandler::~BrowserConnectionHandler() {}

scoped_ptr<BlimpMessageProcessor> BrowserConnectionHandler::RegisterFeature(
    BlimpMessage::Type type,
    BlimpMessageProcessor* incoming_processor) {
  demultiplexer_->AddProcessor(type, incoming_processor);
  return multiplexer_->CreateSenderForType(type);
}

void BrowserConnectionHandler::HandleConnection(
    scoped_ptr<BlimpConnection> connection) {
  DCHECK(connection);
  VLOG(1) << "HandleConnection " << connection;

  if (connection_) {
    DropCurrentConnection();
  }
  connection_ = std::move(connection);

  // Hook up message streams to the connection.
  connection_->SetIncomingMessageProcessor(demultiplexer_.get());
  output_buffer_->SetOutputProcessor(
      connection_->GetOutgoingMessageProcessor());
  connection_->AddConnectionErrorObserver(this);
}

void BrowserConnectionHandler::OnConnectionError(int error) {
  DropCurrentConnection();
}

void BrowserConnectionHandler::DropCurrentConnection() {
  DCHECK(connection_);
  output_buffer_->SetOutputProcessor(nullptr);
  connection_.reset();
}

}  // namespace blimp
