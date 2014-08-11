// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/types.h>
#include <math.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <deque>
#include <map>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/udp/udp_socket.h"

namespace media {
namespace cast {
namespace test {

const size_t kMaxPacketSize = 4096;

class SendToFDPipe : public PacketPipe {
 public:
  explicit SendToFDPipe(int fd) : fd_(fd) {
  }
  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    while (1) {
      int written = write(
          fd_,
          reinterpret_cast<char*>(&packet->front()),
          packet->size());
      if (written < 0) {
        if (errno == EINTR) continue;
        perror("write");
        exit(1);
      }
      if (written != static_cast<int>(packet->size())) {
        fprintf(stderr, "Truncated write!\n");
        exit(1);
      }
      break;
    }
  }
 private:
  int fd_;
};

class QueueManager : public base::MessageLoopForIO::Watcher {
 public:
  QueueManager(int input_fd,
               int output_fd,
               scoped_ptr<PacketPipe> pipe) :
      input_fd_(input_fd),
      packet_pipe_(pipe.Pass()) {

    CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
        input_fd_, true, base::MessageLoopForIO::WATCH_READ,
        &read_socket_watcher_, this));

    scoped_ptr<PacketPipe> tmp(new SendToFDPipe(output_fd));
    if (packet_pipe_) {
      packet_pipe_->AppendToPipe(tmp.Pass());
    } else {
      packet_pipe_ = tmp.Pass();
    }
    packet_pipe_->InitOnIOThread(base::MessageLoopProxy::current(),
                                 &tick_clock_);
  }

  virtual ~QueueManager() {
  }

  // MessageLoopForIO::Watcher methods
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    scoped_ptr<Packet> packet(new Packet(kMaxPacketSize));
    int nread = read(input_fd_,
                     reinterpret_cast<char*>(&packet->front()),
                     kMaxPacketSize);
    if (nread < 0) {
      if (errno == EINTR) return;
      perror("read");
      exit(1);
    }
    if (nread == 0) return;
    packet->resize(nread);
    packet_pipe_->Send(packet.Pass());
  }
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {
    NOTREACHED();
  }

 private:
  int input_fd_;
  scoped_ptr<PacketPipe> packet_pipe_;
  base::MessageLoopForIO::FileDescriptorWatcher read_socket_watcher_;
  base::DefaultTickClock tick_clock_;
};

}  // namespace test
}  // namespace cast
}  // namespace media

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

int tun_alloc(char *dev, int flags) {
  struct ifreq ifr;
  int fd, err;
  const char *clonedev = "/dev/net/tun";

  /* Arguments taken by the function:
   *
   * char *dev: the name of an interface (or '\0'). MUST have enough
   *   space to hold the interface name if '\0' is passed
   * int flags: interface flags (eg, IFF_TUN etc.)
   */

  /* open the clone device */
  if( (fd = open(clonedev, O_RDWR)) < 0 ) {
    return fd;
  }

  /* preparation of the struct ifr, of type "struct ifreq" */
  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

  if (*dev) {
    /* if a device name was specified, put it in the structure; otherwise,
     * the kernel will try to allocate the "next" device of the
     * specified type */
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  /* try to create the device */
  if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
    close(fd);
    return err;
  }

  if (!*dev) {
    /* if the operation was successful, write back the name of the
     * interface to the variable "dev", so the caller can know
     * it. Note that the caller MUST reserve space in *dev (see calling
     * code below) */
    strcpy(dev, ifr.ifr_name);
  }

  /* this is the special file descriptor that the caller will use to talk
   * with the virtual interface */
  return fd;
}


int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  if (argc < 4) {
    fprintf(stderr, "Usage: tap_proxy tap1 tap2 type\n");
    fprintf(stderr,
            "Where 'type' is one of perfect, good, wifi, bad or evil\n");
    exit(1);
  }

  scoped_ptr<media::cast::test::PacketPipe> in_pipe, out_pipe;
  std::string network_type = argv[3];
  if (network_type == "perfect") {
    // No action needed.
  } else if (network_type == "good") {
    in_pipe = media::cast::test::GoodNetwork().Pass();
    out_pipe = media::cast::test::GoodNetwork().Pass();
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

  int fd1 = tun_alloc(argv[1], IFF_TAP);
  int fd2 = tun_alloc(argv[2], IFF_TAP);

  base::MessageLoopForIO message_loop;
  last_printout = base::TimeTicks::Now();
  media::cast::test::QueueManager qm1(fd1, fd2, in_pipe.Pass());
  media::cast::test::QueueManager qm2(fd2, fd1, out_pipe.Pass());
  CheckByteCounters();
  printf("Press Ctrl-C when done.\n");
  message_loop.Run();
}
