// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdlib.h>
#include <vector>

#include "media/cast/test/utility/udp_proxy.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/udp/udp_socket.h"

namespace media {
namespace cast {
namespace test {

const size_t kMaxPacketSize = 65536;

PacketPipe::PacketPipe() {}
PacketPipe::~PacketPipe() {}
void PacketPipe::InitOnIOThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::TickClock* clock) {
  task_runner_ = task_runner;
  clock_ = clock;
  if (pipe_) {
    pipe_->InitOnIOThread(task_runner, clock);
  }
}
void PacketPipe::AppendToPipe(scoped_ptr<PacketPipe> pipe) {
  if (pipe_) {
    pipe_->AppendToPipe(pipe.Pass());
  } else {
    pipe_ = pipe.Pass();
  }
}

// Roughly emulates a buffer inside a device.
// If the buffer is full, packets are dropped.
// Packets are output at a maximum bandwidth.
class Buffer : public PacketPipe {
 public:
  Buffer(size_t buffer_size, double max_megabits_per_second)
      : buffer_size_(0),
        max_buffer_size_(buffer_size),
        max_megabits_per_second_(max_megabits_per_second),
        weak_factory_(this) {
    CHECK_GT(max_buffer_size_, 0UL);
    CHECK_GT(max_megabits_per_second, 0);
  }

  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    if (packet->size() + buffer_size_ <= max_buffer_size_) {
      buffer_size_ += packet->size();
      buffer_.push_back(linked_ptr<Packet>(packet.release()));
      if (buffer_.size() == 1) {
        Schedule();
      }
    }
  }

 private:
  void Schedule() {
    double megabits = buffer_.front()->size() * 8 / 1000000.0;
    double seconds = megabits / max_megabits_per_second_;
    int64 microseconds = static_cast<int64>(seconds * 1E6);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Buffer::ProcessBuffer, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(microseconds));
  }

  void ProcessBuffer() {
    CHECK(!buffer_.empty());
    scoped_ptr<Packet> packet(buffer_.front().release());
    buffer_size_ -= packet->size();
    buffer_.pop_front();
    pipe_->Send(packet.Pass());
    if (!buffer_.empty()) {
      Schedule();
    }
  }

  std::deque<linked_ptr<Packet> > buffer_;
  size_t buffer_size_;
  size_t max_buffer_size_;
  double max_megabits_per_second_;  // megabits per second
  base::WeakPtrFactory<Buffer> weak_factory_;
};

scoped_ptr<PacketPipe> NewBuffer(size_t buffer_size, double bandwidth) {
  return scoped_ptr<PacketPipe>(new Buffer(buffer_size, bandwidth)).Pass();
}

class RandomDrop : public PacketPipe {
 public:
  RandomDrop(double drop_fraction)
      : drop_fraction_(static_cast<int>(drop_fraction * RAND_MAX)) {}

  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    if (rand() > drop_fraction_) {
      pipe_->Send(packet.Pass());
    }
  }

 private:
  int drop_fraction_;
};

scoped_ptr<PacketPipe> NewRandomDrop(double drop_fraction) {
  return scoped_ptr<PacketPipe>(new RandomDrop(drop_fraction)).Pass();
}

class SimpleDelayBase : public PacketPipe {
 public:
  SimpleDelayBase() : weak_factory_(this) {}
  virtual ~SimpleDelayBase() {}

  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    double seconds = GetDelay();
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SimpleDelayBase::SendInternal,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(&packet)),
        base::TimeDelta::FromMicroseconds(static_cast<int64>(seconds * 1E6)));
  }
 protected:
  virtual double GetDelay() = 0;

 private:
  virtual void SendInternal(scoped_ptr<Packet> packet) {
    pipe_->Send(packet.Pass());
  }

  base::WeakPtrFactory<SimpleDelayBase> weak_factory_;
};

class ConstantDelay : public SimpleDelayBase {
 public:
  ConstantDelay(double delay_seconds) : delay_seconds_(delay_seconds) {}
  virtual double GetDelay() OVERRIDE {
    return delay_seconds_;
  }

 private:
  double delay_seconds_;
};

scoped_ptr<PacketPipe> NewConstantDelay(double delay_seconds) {
  return scoped_ptr<PacketPipe>(new ConstantDelay(delay_seconds)).Pass();
}

class RandomUnsortedDelay : public SimpleDelayBase {
 public:
  RandomUnsortedDelay(double random_delay) : random_delay_(random_delay) {}

  virtual double GetDelay() OVERRIDE {
    return random_delay_ * base::RandDouble();
  }

 private:
  double random_delay_;
};

scoped_ptr<PacketPipe> NewRandomUnsortedDelay(double random_delay) {
  return scoped_ptr<PacketPipe>(new RandomUnsortedDelay(random_delay)).Pass();
}


class RandomSortedDelay : public PacketPipe {
 public:
  RandomSortedDelay(double random_delay,
                    double extra_delay,
                    double seconds_between_extra_delay)
      : random_delay_(random_delay),
        extra_delay_(extra_delay),
        seconds_between_extra_delay_(seconds_between_extra_delay),
        weak_factory_(this) {}

  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    buffer_.push_back(linked_ptr<Packet>(packet.release()));
    if (buffer_.size() == 1) {
      Schedule();
    }
  }
  virtual void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::TickClock* clock) OVERRIDE {
    PacketPipe::InitOnIOThread(task_runner, clock);
    // As we start the stream, assume that we are in a random
    // place between two extra delays, thus multiplier = 1.0;
    ScheduleExtraDelay(1.0);
  }

 private:
  void ScheduleExtraDelay(double mult) {
    double seconds = seconds_between_extra_delay_ * mult * base::RandDouble();
    int64 microseconds = static_cast<int64>(seconds * 1E6);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RandomSortedDelay::CauseExtraDelay,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(microseconds));
  }

  void CauseExtraDelay() {
    block_until_ = clock_->NowTicks() +
        base::TimeDelta::FromMicroseconds(
            static_cast<int64>(extra_delay_ * 1E6));
    // An extra delay just happened, wait up to seconds_between_extra_delay_*2
    // before scheduling another one to make the average equal to
    // seconds_between_extra_delay_.
    ScheduleExtraDelay(2.0);
  }

  void Schedule() {
    double seconds = base::RandDouble() * random_delay_;
    base::TimeDelta block_time = block_until_ - base::TimeTicks::Now();
    base::TimeDelta delay_time =
        base::TimeDelta::FromMicroseconds(
            static_cast<int64>(seconds * 1E6));
    if (block_time > delay_time) {
      block_time = delay_time;
    }

    task_runner_->PostDelayedTask(FROM_HERE,
                                  base::Bind(&RandomSortedDelay::ProcessBuffer,
                                             weak_factory_.GetWeakPtr()),
                                  delay_time);
  }

  void ProcessBuffer() {
    CHECK(!buffer_.empty());
    scoped_ptr<Packet> packet(buffer_.front().release());
    pipe_->Send(packet.Pass());
    buffer_.pop_front();
    if (!buffer_.empty()) {
      Schedule();
    }
  }

  base::TimeTicks block_until_;
  std::deque<linked_ptr<Packet> > buffer_;
  double random_delay_;
  double extra_delay_;
  double seconds_between_extra_delay_;
  base::WeakPtrFactory<RandomSortedDelay> weak_factory_;
};

scoped_ptr<PacketPipe> NewRandomSortedDelay(
    double random_delay,
    double extra_delay,
    double seconds_between_extra_delay) {
  return scoped_ptr<PacketPipe>(
             new RandomSortedDelay(
                 random_delay, extra_delay, seconds_between_extra_delay))
      .Pass();
}

class NetworkGlitchPipe : public PacketPipe {
 public:
  NetworkGlitchPipe(double average_work_time, double average_outage_time)
      : works_(false),
        max_work_time_(average_work_time * 2),
        max_outage_time_(average_outage_time * 2),
        weak_factory_(this) {}

  virtual void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::TickClock* clock) OVERRIDE {
    PacketPipe::InitOnIOThread(task_runner, clock);
    Flip();
  }

  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    if (works_) {
      pipe_->Send(packet.Pass());
    }
  }

 private:
  void Flip() {
    works_ = !works_;
    double seconds = base::RandDouble() *
        (works_ ? max_work_time_ : max_outage_time_);
    int64 microseconds = static_cast<int64>(seconds * 1E6);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NetworkGlitchPipe::Flip, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(microseconds));
  }

  bool works_;
  double max_work_time_;
  double max_outage_time_;
  base::WeakPtrFactory<NetworkGlitchPipe> weak_factory_;
};

scoped_ptr<PacketPipe> NewNetworkGlitchPipe(double average_work_time,
                                            double average_outage_time) {
  return scoped_ptr<PacketPipe>(
             new NetworkGlitchPipe(average_work_time, average_outage_time))
      .Pass();
}


// Internal buffer object for a client of the IPP model.
class InterruptedPoissonProcess::InternalBuffer : public PacketPipe {
 public:
  InternalBuffer(base::WeakPtr<InterruptedPoissonProcess> ipp,
                 size_t size)
      : ipp_(ipp),
        stored_size_(0),
        stored_limit_(size),
        clock_(NULL),
        weak_factory_(this) {
  }

  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE {
    // Drop if buffer is full.
    if (stored_size_ >= stored_limit_)
      return;
    stored_size_ += packet->size();
    buffer_.push_back(linked_ptr<Packet>(packet.release()));
    buffer_time_.push_back(clock_->NowTicks());
    DCHECK(buffer_.size() == buffer_time_.size());
  }

  virtual void InitOnIOThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::TickClock* clock) OVERRIDE {
    clock_ = clock;
    if (ipp_)
      ipp_->InitOnIOThread(task_runner, clock);
    PacketPipe::InitOnIOThread(task_runner, clock);
  }

  void SendOnePacket() {
    scoped_ptr<Packet> packet(buffer_.front().release());
    stored_size_ -= packet->size();
    buffer_.pop_front();
    buffer_time_.pop_front();
    pipe_->Send(packet.Pass());
    DCHECK(buffer_.size() == buffer_time_.size());
  }

  bool Empty() const {
    return buffer_.empty();
  }

  base::TimeTicks FirstPacketTime() const {
    DCHECK(!buffer_time_.empty());
    return buffer_time_.front();
  }

  base::WeakPtr<InternalBuffer> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();

  }

 private:
  const base::WeakPtr<InterruptedPoissonProcess> ipp_;
  size_t stored_size_;
  const size_t stored_limit_;
  std::deque<linked_ptr<Packet> > buffer_;
  std::deque<base::TimeTicks> buffer_time_;
  base::TickClock* clock_;
  base::WeakPtrFactory<InternalBuffer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InternalBuffer);
};

InterruptedPoissonProcess::InterruptedPoissonProcess(
    const std::vector<double>& average_rates,
    double coef_burstiness,
    double coef_variance,
    uint32 rand_seed)
    : clock_(NULL),
      average_rates_(average_rates),
      coef_burstiness_(coef_burstiness),
      coef_variance_(coef_variance),
      rate_index_(0),
      on_state_(true),
      weak_factory_(this) {
  mt_rand_.init_genrand(rand_seed);
  DCHECK(!average_rates.empty());
  ComputeRates();
}

InterruptedPoissonProcess::~InterruptedPoissonProcess() {
}

void InterruptedPoissonProcess::InitOnIOThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::TickClock* clock) {
  // Already initialized and started.
  if (task_runner_ &&  clock_)
    return;
  task_runner_ = task_runner;
  clock_ = clock;
  UpdateRates();
  SwitchOn();
  SendPacket();
}

scoped_ptr<PacketPipe> InterruptedPoissonProcess::NewBuffer(size_t size) {
  scoped_ptr<InternalBuffer> buffer(
      new InternalBuffer(weak_factory_.GetWeakPtr(), size));
  send_buffers_.push_back(buffer->GetWeakPtr());
  return buffer.PassAs<PacketPipe>();
}

base::TimeDelta InterruptedPoissonProcess::NextEvent(double rate) {
  // Rate is per milliseconds.
  // The time until next event is exponentially distributed to the
  // inverse of |rate|.
  return base::TimeDelta::FromMillisecondsD(
      fabs(-log(1.0 - RandDouble()) / rate));
}

double InterruptedPoissonProcess::RandDouble() {
  // Generate a 64-bits random number from MT19937 and then convert
  // it to double.
  uint64 rand = mt_rand_.genrand_int32();
  rand <<= 32;
  rand |= mt_rand_.genrand_int32();
  return base::BitsToOpenEndedUnitInterval(rand);
}

void InterruptedPoissonProcess::ComputeRates() {
  double avg_rate = average_rates_[rate_index_];

  send_rate_ = avg_rate / coef_burstiness_;
  switch_off_rate_ =
      2 * avg_rate * (1 - coef_burstiness_) * (1 - coef_burstiness_) /
      coef_burstiness_ / (coef_variance_ - 1);
  switch_on_rate_ =
      2 * avg_rate * (1 - coef_burstiness_) / (coef_variance_ - 1);
}

void InterruptedPoissonProcess::UpdateRates() {
  ComputeRates();

  // Rates are updated once per second.
  rate_index_ = (rate_index_ + 1) % average_rates_.size();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&InterruptedPoissonProcess::UpdateRates,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

void InterruptedPoissonProcess::SwitchOff() {
  on_state_ = false;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&InterruptedPoissonProcess::SwitchOn,
                 weak_factory_.GetWeakPtr()),
      NextEvent(switch_on_rate_));
}

void InterruptedPoissonProcess::SwitchOn() {
  on_state_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&InterruptedPoissonProcess::SwitchOff,
                 weak_factory_.GetWeakPtr()),
      NextEvent(switch_off_rate_));
}

void InterruptedPoissonProcess::SendPacket() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&InterruptedPoissonProcess::SendPacket,
                 weak_factory_.GetWeakPtr()),
      NextEvent(send_rate_));

  // If OFF then don't send.
  if (!on_state_)
    return;

  // Find the earliest packet to send.
  base::TimeTicks earliest_time;
  for (size_t i = 0; i < send_buffers_.size(); ++i) {
    if (!send_buffers_[i])
      continue;
    if (send_buffers_[i]->Empty())
      continue;
    if (earliest_time.is_null() ||
        send_buffers_[i]->FirstPacketTime() < earliest_time)
      earliest_time = send_buffers_[i]->FirstPacketTime();
  }
  for (size_t i = 0; i < send_buffers_.size(); ++i) {
    if (!send_buffers_[i])
      continue;
    if (send_buffers_[i]->Empty())
      continue;
    if (send_buffers_[i]->FirstPacketTime() != earliest_time)
      continue;
    send_buffers_[i]->SendOnePacket();
    break;
  }
}

class UDPProxyImpl;

class PacketSender : public PacketPipe {
 public:
  PacketSender(UDPProxyImpl* udp_proxy, const net::IPEndPoint* destination)
      : udp_proxy_(udp_proxy), destination_(destination) {}
  virtual void Send(scoped_ptr<Packet> packet) OVERRIDE;
  virtual void AppendToPipe(scoped_ptr<PacketPipe> pipe) OVERRIDE {
    NOTREACHED();
  }

 private:
  UDPProxyImpl* udp_proxy_;
  const net::IPEndPoint* destination_;  // not owned
};

namespace {
void BuildPipe(scoped_ptr<PacketPipe>* pipe, PacketPipe* next) {
  if (*pipe) {
    (*pipe)->AppendToPipe(scoped_ptr<PacketPipe>(next).Pass());
  } else {
    pipe->reset(next);
  }
}
}  // namespace

scoped_ptr<PacketPipe> WifiNetwork() {
  // This represents the buffer on the sender.
  scoped_ptr<PacketPipe> pipe;
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  BuildPipe(&pipe, new RandomDrop(0.005));
  // This represents the buffer on the router.
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  BuildPipe(&pipe, new RandomSortedDelay(1E-3, 20E-3, 3));
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  BuildPipe(&pipe, new RandomSortedDelay(1E-3, 20E-3, 3));
  BuildPipe(&pipe, new RandomDrop(0.005));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(256 << 10, 20));
  return pipe.Pass();
}

scoped_ptr<PacketPipe> BadNetwork() {
  scoped_ptr<PacketPipe> pipe;
  // This represents the buffer on the sender.
  BuildPipe(&pipe, new Buffer(64 << 10, 5)); // 64 kb buf, 5mbit/s
  BuildPipe(&pipe, new RandomDrop(0.05));  // 5% packet drop
  BuildPipe(&pipe, new RandomSortedDelay(2E-3, 20E-3, 1));
  // This represents the buffer on the router.
  BuildPipe(&pipe, new Buffer(64 << 10, 5));  // 64 kb buf, 4mbit/s
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  // Random 40ms every other second
  //  BuildPipe(&pipe, new NetworkGlitchPipe(2, 40E-1));
  BuildPipe(&pipe, new RandomUnsortedDelay(5E-3));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(64 << 10, 5));  // 64 kb buf, 5mbit/s
  return pipe.Pass();
}


scoped_ptr<PacketPipe> EvilNetwork() {
  // This represents the buffer on the sender.
  scoped_ptr<PacketPipe> pipe;
  BuildPipe(&pipe, new Buffer(4 << 10, 5));  // 4 kb buf, 2mbit/s
  // This represents the buffer on the router.
  BuildPipe(&pipe, new RandomDrop(0.1));  // 10% packet drop
  BuildPipe(&pipe, new RandomSortedDelay(20E-3, 60E-3, 1));
  BuildPipe(&pipe, new Buffer(4 << 10, 2));  // 4 kb buf, 2mbit/s
  BuildPipe(&pipe, new RandomDrop(0.1));  // 10% packet drop
  BuildPipe(&pipe, new ConstantDelay(1E-3));
  BuildPipe(&pipe, new NetworkGlitchPipe(2.0, 0.3));
  BuildPipe(&pipe, new RandomUnsortedDelay(20E-3));
  // This represents the buffer on the receiving device.
  BuildPipe(&pipe, new Buffer(4 << 10, 2));  // 4 kb buf, 2mbit/s
  return pipe.Pass();
}

class UDPProxyImpl : public UDPProxy {
 public:
  UDPProxyImpl(const net::IPEndPoint& local_port,
               const net::IPEndPoint& destination,
               scoped_ptr<PacketPipe> to_dest_pipe,
               scoped_ptr<PacketPipe> from_dest_pipe,
               net::NetLog* net_log)
      : local_port_(local_port),
        destination_(destination),
        destination_is_mutable_(destination.address().empty()),
        proxy_thread_("media::cast::test::UdpProxy Thread"),
        to_dest_pipe_(to_dest_pipe.Pass()),
        from_dest_pipe_(from_dest_pipe.Pass()),
        blocked_(false),
        weak_factory_(this) {
    proxy_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    base::WaitableEvent start_event(false, false);
    proxy_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&UDPProxyImpl::Start,
                   base::Unretained(this),
                   base::Unretained(&start_event),
                   net_log));
    start_event.Wait();
  }

  virtual ~UDPProxyImpl() {
    base::WaitableEvent stop_event(false, false);
    proxy_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&UDPProxyImpl::Stop,
                   base::Unretained(this),
                   base::Unretained(&stop_event)));
    stop_event.Wait();
    proxy_thread_.Stop();
  }

  void Send(scoped_ptr<Packet> packet,
            const net::IPEndPoint& destination) {
    if (blocked_) {
      LOG(ERROR) << "Cannot write packet right now: blocked";
      return;
    }

    VLOG(1) << "Sending packet, len = " << packet->size();
    // We ignore all problems, callbacks and errors.
    // If it didn't work we just drop the packet at and call it a day.
    scoped_refptr<net::IOBuffer> buf =
        new net::WrappedIOBuffer(reinterpret_cast<char*>(&packet->front()));
    size_t buf_size = packet->size();
    int result;
    if (destination.address().empty()) {
      VLOG(1) << "Destination has not been set yet.";
      result = net::ERR_INVALID_ARGUMENT;
    } else {
      VLOG(1) << "Destination:" << destination.ToString();
      result = socket_->SendTo(buf,
                               static_cast<int>(buf_size),
                               destination,
                               base::Bind(&UDPProxyImpl::AllowWrite,
                                          weak_factory_.GetWeakPtr(),
                                          buf,
                                          base::Passed(&packet)));
    }
    if (result == net::ERR_IO_PENDING) {
      blocked_ = true;
    } else if (result < 0) {
      LOG(ERROR) << "Failed to write packet.";
    }
  }

 private:
  void Start(base::WaitableEvent* start_event,
             net::NetLog* net_log) {
    socket_.reset(new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                                     net::RandIntCallback(),
                                     net_log,
                                     net::NetLog::Source()));
    BuildPipe(&to_dest_pipe_, new PacketSender(this, &destination_));
    BuildPipe(&from_dest_pipe_, new PacketSender(this, &return_address_));
    to_dest_pipe_->InitOnIOThread(base::MessageLoopProxy::current(),
                                  &tick_clock_);
    from_dest_pipe_->InitOnIOThread(base::MessageLoopProxy::current(),
                                    &tick_clock_);

    VLOG(0) << "From:" << local_port_.ToString();
    if (!destination_is_mutable_)
      VLOG(0) << "To:" << destination_.ToString();

    CHECK_GE(socket_->Bind(local_port_), 0);

    start_event->Signal();
    PollRead();
  }

  void Stop(base::WaitableEvent* stop_event) {
    to_dest_pipe_.reset(NULL);
    from_dest_pipe_.reset(NULL);
    socket_.reset(NULL);
    stop_event->Signal();
  }

  void ProcessPacket(scoped_refptr<net::IOBuffer> recv_buf, int len) {
    DCHECK_NE(len, net::ERR_IO_PENDING);
    VLOG(1) << "Got packet, len = " << len;
    if (len < 0) {
      LOG(WARNING) << "Socket read error: " << len;
      return;
    }
    packet_->resize(len);
    if (destination_is_mutable_ && set_destination_next_ &&
        !(recv_address_ == return_address_) &&
        !(recv_address_ == destination_)) {
      destination_ = recv_address_;
    }
    if (recv_address_ == destination_) {
      set_destination_next_ = false;
      from_dest_pipe_->Send(packet_.Pass());
    } else {
      set_destination_next_ = true;
      VLOG(1) << "Return address = " << recv_address_.ToString();
      return_address_ = recv_address_;
      to_dest_pipe_->Send(packet_.Pass());
    }
  }

  void ReadCallback(scoped_refptr<net::IOBuffer> recv_buf, int len) {
    ProcessPacket(recv_buf, len);
    PollRead();
  }

  void PollRead() {
    while (true) {
      packet_.reset(new Packet(kMaxPacketSize));
      scoped_refptr<net::IOBuffer> recv_buf =
          new net::WrappedIOBuffer(reinterpret_cast<char*>(&packet_->front()));
      int len = socket_->RecvFrom(
          recv_buf,
          kMaxPacketSize,
          &recv_address_,
          base::Bind(&UDPProxyImpl::ReadCallback,
                     base::Unretained(this),
                     recv_buf));
      if (len == net::ERR_IO_PENDING)
        break;
      ProcessPacket(recv_buf, len);
    }
  }

  void AllowWrite(scoped_refptr<net::IOBuffer> buf,
                  scoped_ptr<Packet> packet,
                  int unused_len) {
    DCHECK(blocked_);
    blocked_ = false;
  }

  // Input
  net::IPEndPoint local_port_;

  net::IPEndPoint destination_;
  bool destination_is_mutable_;

  net::IPEndPoint return_address_;
  bool set_destination_next_;

  base::DefaultTickClock tick_clock_;
  base::Thread proxy_thread_;
  scoped_ptr<net::UDPSocket> socket_;
  scoped_ptr<PacketPipe> to_dest_pipe_;
  scoped_ptr<PacketPipe> from_dest_pipe_;

  // For receiving.
  net::IPEndPoint recv_address_;
  scoped_ptr<Packet> packet_;

  // For sending.
  bool blocked_;

  base::WeakPtrFactory<UDPProxyImpl> weak_factory_;
};

void PacketSender::Send(scoped_ptr<Packet> packet) {
  udp_proxy_->Send(packet.Pass(), *destination_);
}

scoped_ptr<UDPProxy> UDPProxy::Create(
    const net::IPEndPoint& local_port,
    const net::IPEndPoint& destination,
    scoped_ptr<PacketPipe> to_dest_pipe,
    scoped_ptr<PacketPipe> from_dest_pipe,
    net::NetLog* net_log) {
  scoped_ptr<UDPProxy> ret(new UDPProxyImpl(local_port,
                                            destination,
                                            to_dest_pipe.Pass(),
                                            from_dest_pipe.Pass(),
                                            net_log));
  return ret.Pass();
}

}  // namespace test
}  // namespace cast
}  // namespace media
