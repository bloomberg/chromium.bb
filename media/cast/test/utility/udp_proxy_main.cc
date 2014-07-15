// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/test/utility/udp_proxy.h"

base::TimeTicks last_printout;

class ByteCounter {
 public:
  ByteCounter() : bytes_(0), packets_(0) {
    push(base::TimeTicks::Now());
  }

  base::TimeDelta time_range() {
    return time_data_.back() - time_data_.front();
  }

  void push(base::TimeTicks now) {
    byte_data_.push_back(bytes_);
    packet_data_.push_back(packets_);
    time_data_.push_back(now);
    while (time_range().InSeconds() > 10) {
      byte_data_.pop_front();
      packet_data_.pop_front();
      time_data_.pop_front();
    }
  }

  double megabits_per_second() {
    double megabits = (byte_data_.back() - byte_data_.front()) * 8 / 1E6;
    return megabits / time_range().InSecondsF();
  }

  double packets_per_second() {
    double packets = packet_data_.back()- packet_data_.front();
    return packets / time_range().InSecondsF();
  }

  void Increment(uint64 x) {
    bytes_ += x;
    packets_ ++;
  }

 private:
  uint64 bytes_;
  uint64 packets_;
  std::deque<uint64> byte_data_;
  std::deque<uint64> packet_data_;
  std::deque<base::TimeTicks> time_data_;
};

ByteCounter in_pipe_input_counter;
ByteCounter in_pipe_output_counter;
ByteCounter out_pipe_input_counter;
ByteCounter out_pipe_output_counter;

class ByteCounterPipe : public media::cast::test::PacketPipe {
 public:
  ByteCounterPipe(ByteCounter* counter) : counter_(counter) {}
  virtual void Send(scoped_ptr<media::cast::Packet> packet)
      OVERRIDE {
    counter_->Increment(packet->size());
    pipe_->Send(packet.Pass());
  }
 private:
  ByteCounter* counter_;
};

void SetupByteCounters(scoped_ptr<media::cast::test::PacketPipe>* pipe,
                       ByteCounter* pipe_input_counter,
                       ByteCounter* pipe_output_counter) {
  media::cast::test::PacketPipe* new_pipe =
      new ByteCounterPipe(pipe_input_counter);
  new_pipe->AppendToPipe(pipe->Pass());
  new_pipe->AppendToPipe(
      scoped_ptr<media::cast::test::PacketPipe>(
          new ByteCounterPipe(pipe_output_counter)).Pass());
  pipe->reset(new_pipe);
}

void CheckByteCounters() {
  base::TimeTicks now = base::TimeTicks::Now();
  in_pipe_input_counter.push(now);
  in_pipe_output_counter.push(now);
  out_pipe_input_counter.push(now);
  out_pipe_output_counter.push(now);
  if ((now - last_printout).InSeconds() >= 5) {
    fprintf(stderr, "Sending  : %5.2f / %5.2f mbps %6.2f / %6.2f packets / s\n",
            in_pipe_output_counter.megabits_per_second(),
            in_pipe_input_counter.megabits_per_second(),
            in_pipe_output_counter.packets_per_second(),
            in_pipe_input_counter.packets_per_second());
    fprintf(stderr, "Receiving: %5.2f / %5.2f mbps %6.2f / %6.2f packets / s\n",
            out_pipe_output_counter.megabits_per_second(),
            out_pipe_input_counter.megabits_per_second(),
            out_pipe_output_counter.packets_per_second(),
            out_pipe_input_counter.packets_per_second());

    last_printout = now;
  }
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CheckByteCounters),
      base::TimeDelta::FromMilliseconds(100));
}

int main(int argc, char** argv) {
  if (argc != 5 && argc != 3) {
    fprintf(stderr,
            "Usage: udp_proxy <localport> <remotehost> <remoteport> <type>\n"
            "or:\n"
            "       udp_proxy <localport> <type>\n"
            "Where type is one of: perfect, wifi, bad, evil\n");
    exit(1);
  }

  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  net::IPAddressNumber remote_ip_number;
  net::IPAddressNumber local_ip_number;
  std::string network_type;
  int local_port = atoi(argv[1]);
  int remote_port = 0;
  CHECK(net::ParseIPLiteralToNumber("0.0.0.0", &local_ip_number));

  if (argc == 5) {
    // V2 proxy
    CHECK(net::ParseIPLiteralToNumber(argv[2], &remote_ip_number));
    remote_port = atoi(argv[3]);
    network_type = argv[4];
  } else {
    // V1 proxy
    network_type = argv[2];
  }
  net::IPEndPoint remote_endpoint(remote_ip_number, remote_port);
  net::IPEndPoint local_endpoint(local_ip_number, local_port);
  scoped_ptr<media::cast::test::PacketPipe> in_pipe, out_pipe;

  if (network_type == "perfect") {
    // No action needed.
  } else if (network_type == "wifi") {
    in_pipe = media::cast::test::WifiNetwork().Pass();
    out_pipe = media::cast::test::WifiNetwork().Pass();
  } else if (network_type == "bad") {
    in_pipe = media::cast::test::BadNetwork().Pass();
    out_pipe = media::cast::test::BadNetwork().Pass();
  } else if (network_type == "evil") {
    in_pipe = media::cast::test::EvilNetwork().Pass();
    out_pipe = media::cast::test::EvilNetwork().Pass();
  } else {
    fprintf(stderr, "Unknown network type.\n");
    exit(1);
  }

  SetupByteCounters(&in_pipe, &in_pipe_input_counter, &in_pipe_output_counter);
  SetupByteCounters(
      &out_pipe, &out_pipe_input_counter, &out_pipe_output_counter);

  printf("Press Ctrl-C when done.\n");
  scoped_ptr<media::cast::test::UDPProxy> proxy(
      media::cast::test::UDPProxy::Create(local_endpoint,
                                          remote_endpoint,
                                          in_pipe.Pass(),
                                          out_pipe.Pass(),
                                          NULL));
  base::MessageLoop message_loop;
  last_printout = base::TimeTicks::Now();
  CheckByteCounters();
  message_loop.Run();
  return 1;
}
