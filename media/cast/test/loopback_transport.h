// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_
#define MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_config.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}  // namespace base

namespace media {
namespace cast {

namespace test {
class PacketPipe;
}  // namespace test

// Class that sends the packet to a receiver through a stack of PacketPipes.
class LoopBackTransport : public PacketSender {
 public:
  explicit LoopBackTransport(
      scoped_refptr<CastEnvironment> cast_environment);
  virtual ~LoopBackTransport();

  virtual bool SendPacket(PacketRef packet,
                          const base::Closure& cb) OVERRIDE;

  virtual int64 GetBytesSent() OVERRIDE;

  // Initiailize this loopback transport.
  // Establish a flow of packets from |pipe| to |packet_receiver|.
  // The data flow looks like:
  // SendPacket() -> |pipe| -> Fake loopback pipe -> |packet_receiver|.
  void Initialize(
      scoped_ptr<test::PacketPipe> pipe,
      const PacketReceiverCallback& packet_receiver,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::TickClock* clock);

 private:
  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<test::PacketPipe> packet_pipe_;
  int64 bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(LoopBackTransport);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_
