// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_connection.h"

#include "base/macros.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_pump.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/packet_reader.h"
#include "blimp/net/packet_writer.h"

namespace blimp {

BlimpConnection::BlimpConnection(scoped_ptr<PacketReader> reader,
                                 scoped_ptr<PacketWriter> writer)
    : reader_(reader.Pass()),
      message_pump_(new BlimpMessagePump(reader_.get())),
      writer_(writer.Pass()) {
  DCHECK(writer_);
}

BlimpConnection::~BlimpConnection() {}

void BlimpConnection::SetConnectionErrorObserver(
    ConnectionErrorObserver* observer) {
  message_pump_->set_error_observer(observer);
}

void BlimpConnection::SetIncomingMessageProcessor(
    BlimpMessageProcessor* processor) {
  message_pump_->SetMessageProcessor(processor);
}

}  // namespace blimp
