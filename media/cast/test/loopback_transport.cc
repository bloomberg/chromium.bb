// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/loopback_transport.h"

#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "media/cast/test/utility/udp_proxy.h"

namespace media {
namespace cast {
namespace {

// Shim that turns forwards packets from a test::PacketPipe to a
// PacketReceiverCallback.
class LoopBackPacketPipe : public test::PacketPipe {
 public:
  LoopBackPacketPipe(
      const PacketReceiverCallback& packet_receiver)
      : packet_receiver_(packet_receiver) {}

  virtual ~LoopBackPacketPipe() {}

  // PacketPipe implementations.
  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    packet_receiver_.Run(packet.Pass());
  }

 private:
  PacketReceiverCallback packet_receiver_;

  DISALLOW_COPY_AND_ASSIGN(LoopBackPacketPipe);
};

}  // namespace

LoopBackTransport::LoopBackTransport(
    scoped_refptr<CastEnvironment> cast_environment)
    : cast_environment_(cast_environment) {
}

LoopBackTransport::~LoopBackTransport() {
}

bool LoopBackTransport::SendPacket(PacketRef packet,
                                   const base::Closure& cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  scoped_ptr<Packet> packet_copy(new Packet(packet->data));
  packet_pipe_->Send(packet_copy.Pass());
  return true;
}

void LoopBackTransport::Initialize(
    scoped_ptr<test::PacketPipe> pipe,
    const PacketReceiverCallback& packet_receiver,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::TickClock* clock) {
  scoped_ptr<test::PacketPipe> loopback_pipe(
      new LoopBackPacketPipe(packet_receiver));
  // Append the loopback pipe to the end.
  pipe->AppendToPipe(loopback_pipe.Pass());
  packet_pipe_ = pipe.Pass();
  packet_pipe_->InitOnIOThread(task_runner, clock);
}

}  // namespace cast
}  // namespace media
