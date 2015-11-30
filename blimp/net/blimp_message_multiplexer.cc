// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_multiplexer.h"

#include "base/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"

namespace blimp {
namespace {

class MultiplexedSender : public BlimpMessageProcessor {
 public:
  MultiplexedSender(base::WeakPtr<BlimpMessageProcessor> output_processor,
                    BlimpMessage::Type type);
  ~MultiplexedSender() override;

  // BlimpMessageProcessor implementation.
  // |message.type|, if set, must match the sender's type.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  base::WeakPtr<BlimpMessageProcessor> output_processor_;
  BlimpMessage::Type type_;

  DISALLOW_COPY_AND_ASSIGN(MultiplexedSender);
};

MultiplexedSender::MultiplexedSender(
    base::WeakPtr<BlimpMessageProcessor> output_processor,
    BlimpMessage::Type type)
    : output_processor_(output_processor), type_(type) {}

MultiplexedSender::~MultiplexedSender() {}

void MultiplexedSender::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  if (message->has_type()) {
    DCHECK_EQ(type_, message->type());
  } else {
    message->set_type(type_);
  }
  output_processor_->ProcessMessage(std::move(message), callback);
}

}  // namespace

BlimpMessageMultiplexer::BlimpMessageMultiplexer(
    BlimpMessageProcessor* output_processor)
    : output_weak_factory_(output_processor) {}

BlimpMessageMultiplexer::~BlimpMessageMultiplexer() {}

scoped_ptr<BlimpMessageProcessor> BlimpMessageMultiplexer::CreateSenderForType(
    BlimpMessage::Type type) {
  return make_scoped_ptr(
      new MultiplexedSender(output_weak_factory_.GetWeakPtr(), type));
}
}  // namespace blimp
