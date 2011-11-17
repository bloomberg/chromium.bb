// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/pseudotcp_adapter.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/udp/udp_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace jingle_glue {
namespace {
class FakeSocket;
}  // namespace
}  // namespace jingle_glue

namespace jingle_glue {

namespace {

// The range is chosen arbitrarily. It must be big enough so that we
// always have at least two UDP ports available.
const int kMinPort = 32000;
const int kMaxPort = 33000;

const int kMessageSize = 1024;
const int kMessages = 100;
const int kTestDataSize = kMessages * kMessageSize;

class RateLimiter {
 public:
  virtual ~RateLimiter() { };
  // Returns true if the new packet needs to be dropped, false otherwise.
  virtual bool DropNextPacket() = 0;
};

class LeakyBucket : public RateLimiter {
 public:
  // |rate| is in drops per second.
  LeakyBucket(double volume, double rate)
      : volume_(volume),
        rate_(rate),
        level_(0.0),
        last_update_(base::TimeTicks::HighResNow()) {
  }

  virtual ~LeakyBucket() { }

  virtual bool DropNextPacket() OVERRIDE {
    base::TimeTicks now = base::TimeTicks::HighResNow();
    double interval = (now - last_update_).InSecondsF();
    last_update_ = now;
    level_ = level_ + 1.0 - interval * rate_;
    if (level_ > volume_) {
      level_ = volume_;
      return true;
    } else if (level_ < 0.0) {
      level_ = 0.0;
    }
    return false;
  }

 private:
  double volume_;
  double rate_;
  double level_;
  base::TimeTicks last_update_;
};

class FakeSocket : public net::Socket {
 public:
  FakeSocket()
      : read_callback_(NULL),
        rate_limiter_(NULL),
        latency_ms_(0) {
  }
  virtual ~FakeSocket() { }

  void AppendInputPacket(const std::vector<char>& data) {
    if (rate_limiter_ && rate_limiter_->DropNextPacket())
      return;  // Lose the packet.

    if (read_callback_) {
      int size = std::min(read_buffer_size_, static_cast<int>(data.size()));
      memcpy(read_buffer_->data(), &data[0], data.size());
      net::OldCompletionCallback* cb = read_callback_;
      read_callback_ = NULL;
      read_buffer_ = NULL;
      cb->Run(size);
    } else {
      incoming_packets_.push_back(data);
    }
  }

  void Connect(FakeSocket* peer_socket) {
    peer_socket_ = peer_socket;
  }

  void set_rate_limiter(RateLimiter* rate_limiter) {
    rate_limiter_ = rate_limiter;
  };

  void set_latency(int latency_ms) { latency_ms_ = latency_ms; };

  // net::Socket interface.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) {
    CHECK(!read_callback_);
    CHECK(buf);

    if (incoming_packets_.size() > 0) {
      scoped_refptr<net::IOBuffer> buffer(buf);
      int size = std::min(
          static_cast<int>(incoming_packets_.front().size()), buf_len);
      memcpy(buffer->data(), &*incoming_packets_.front().begin(), size);
      incoming_packets_.pop_front();
      return size;
    } else {
      read_callback_ = callback;
      read_buffer_ = buf;
      read_buffer_size_ = buf_len;
      return net::ERR_IO_PENDING;
    }
  }

  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) OVERRIDE {
    DCHECK(buf);
    if (peer_socket_) {
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&FakeSocket::AppendInputPacket,
                     base::Unretained(peer_socket_),
                     std::vector<char>(buf->data(), buf->data() + buf_len)),
          latency_ms_);
    }

    return buf_len;
  }

  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual bool SetSendBufferSize(int32 size) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }

 private:
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::OldCompletionCallback* read_callback_;

  std::deque<std::vector<char> > incoming_packets_;

  FakeSocket* peer_socket_;
  RateLimiter* rate_limiter_;
  int latency_ms_;
};

class TCPChannelTester : public base::RefCountedThreadSafe<TCPChannelTester> {
 public:
  TCPChannelTester(MessageLoop* message_loop,
                   net::Socket* client_socket,
                   net::Socket* host_socket)
      : message_loop_(message_loop),
        host_socket_(host_socket),
        client_socket_(client_socket),
        done_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &TCPChannelTester::OnWritten)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &TCPChannelTester::OnRead)),
        write_errors_(0),
        read_errors_(0) {
  }

  virtual ~TCPChannelTester() { }

  void Start() {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&TCPChannelTester::DoStart, this));
  }

  void CheckResults() {
    EXPECT_EQ(0, write_errors_);
    EXPECT_EQ(0, read_errors_);

    ASSERT_EQ(kTestDataSize + kMessageSize, input_buffer_->capacity());

    output_buffer_->SetOffset(0);
    ASSERT_EQ(kTestDataSize, output_buffer_->size());

    EXPECT_EQ(0, memcmp(output_buffer_->data(),
                        input_buffer_->StartOfBuffer(), kTestDataSize));
  }

 protected:
  void Done() {
    done_ = true;
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  void DoStart() {
    InitBuffers();
    DoRead();
    DoWrite();
  }

  void InitBuffers() {
    output_buffer_ = new net::DrainableIOBuffer(
        new net::IOBuffer(kTestDataSize), kTestDataSize);
    memset(output_buffer_->data(), 123, kTestDataSize);

    input_buffer_ = new net::GrowableIOBuffer();
    // Always keep kMessageSize bytes available at the end of the input buffer.
    input_buffer_->SetCapacity(kMessageSize);
  }

  void DoWrite() {
    int result = 1;
    while (result > 0) {
      if (output_buffer_->BytesRemaining() == 0)
        break;

      int bytes_to_write = std::min(output_buffer_->BytesRemaining(),
                                    kMessageSize);
      result = client_socket_->Write(output_buffer_, bytes_to_write,
                                     &write_cb_);
      HandleWriteResult(result);
    }
  }

  void OnWritten(int result) {
    HandleWriteResult(result);
    DoWrite();
  }

  void HandleWriteResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Received error " << result << " when trying to write";
      write_errors_++;
      Done();
    } else if (result > 0) {
      output_buffer_->DidConsume(result);
    }
  }

  void DoRead() {
    int result = 1;
    while (result > 0) {
      input_buffer_->set_offset(input_buffer_->capacity() - kMessageSize);

      result = host_socket_->Read(input_buffer_, kMessageSize, &read_cb_);
      HandleReadResult(result);
    };
  }

  void OnRead(int result) {
    HandleReadResult(result);
    DoRead();
  }

  void HandleReadResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      if (!done_) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        Done();
      }
    } else if (result > 0) {
      // Allocate memory for the next read.
      input_buffer_->SetCapacity(input_buffer_->capacity() + result);
      if (input_buffer_->capacity() == kTestDataSize + kMessageSize)
        Done();
    }
  }

 private:
  MessageLoop* message_loop_;
  net::Socket* host_socket_;
  net::Socket* client_socket_;
  bool done_;

  scoped_refptr<net::DrainableIOBuffer> output_buffer_;
  scoped_refptr<net::GrowableIOBuffer> input_buffer_;

  net::OldCompletionCallbackImpl<TCPChannelTester> write_cb_;
  net::OldCompletionCallbackImpl<TCPChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
};

class PseudoTcpAdapterTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    JingleThreadWrapper::EnsureForCurrentThread();

    host_socket_ = new FakeSocket();
    client_socket_ = new FakeSocket();

    host_socket_->Connect(client_socket_);
    client_socket_->Connect(host_socket_);

    host_pseudotcp_.reset(new PseudoTcpAdapter(host_socket_));
    client_pseudotcp_.reset(new PseudoTcpAdapter(client_socket_));
  }

  FakeSocket* host_socket_;
  FakeSocket* client_socket_;

  scoped_ptr<PseudoTcpAdapter> host_pseudotcp_;
  scoped_ptr<PseudoTcpAdapter> client_pseudotcp_;
  MessageLoop message_loop_;
};

TEST_F(PseudoTcpAdapterTest, DataTransfer) {
  TestOldCompletionCallback host_connect_cb;
  TestOldCompletionCallback client_connect_cb;

  int rv1 = host_pseudotcp_->Connect(&host_connect_cb);
  int rv2 = client_pseudotcp_->Connect(&client_connect_cb);

  if (rv1 == net::ERR_IO_PENDING)
    rv1 = host_connect_cb.WaitForResult();
  if (rv2 == net::ERR_IO_PENDING)
    rv2 = client_connect_cb.WaitForResult();
  ASSERT_EQ(net::OK, rv1);
  ASSERT_EQ(net::OK, rv2);

  scoped_refptr<TCPChannelTester> tester =
      new TCPChannelTester(&message_loop_, host_pseudotcp_.get(),
                           client_pseudotcp_.get());

  tester->Start();
  message_loop_.Run();
  tester->CheckResults();
}

TEST_F(PseudoTcpAdapterTest, LimitedChannel) {
  const int kLatencyMs = 20;
  const int kPacketsPerSecond = 400;
  const int kBurstPackets = 10;

  LeakyBucket host_limiter(kBurstPackets, kPacketsPerSecond);
  host_socket_->set_latency(kLatencyMs);
  host_socket_->set_rate_limiter(&host_limiter);

  LeakyBucket client_limiter(kBurstPackets, kPacketsPerSecond);
  host_socket_->set_latency(kLatencyMs);
  client_socket_->set_rate_limiter(&client_limiter);

  TestOldCompletionCallback host_connect_cb;
  TestOldCompletionCallback client_connect_cb;

  int rv1 = host_pseudotcp_->Connect(&host_connect_cb);
  int rv2 = client_pseudotcp_->Connect(&client_connect_cb);

  if (rv1 == net::ERR_IO_PENDING)
    rv1 = host_connect_cb.WaitForResult();
  if (rv2 == net::ERR_IO_PENDING)
    rv2 = client_connect_cb.WaitForResult();
  ASSERT_EQ(net::OK, rv1);
  ASSERT_EQ(net::OK, rv2);

  scoped_refptr<TCPChannelTester> tester =
      new TCPChannelTester(&message_loop_, host_pseudotcp_.get(),
                           client_pseudotcp_.get());

  tester->Start();
  message_loop_.Run();
  tester->CheckResults();
}

class DeleteOnConnected {
 public:
  DeleteOnConnected(MessageLoop* message_loop,
                    scoped_ptr<PseudoTcpAdapter>* adapter)
      : message_loop_(message_loop), adapter_(adapter) {}
  void OnConnected(int error) {
    adapter_->reset();
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }
  MessageLoop* message_loop_;
  scoped_ptr<PseudoTcpAdapter>* adapter_;
};

TEST_F(PseudoTcpAdapterTest, DeleteOnConnected) {
  // This test verifies that deleting the adapter mid-callback doesn't lead
  // to deleted structures being touched as the stack unrolls, so the failure
  // mode is a crash rather than a normal test failure.
  TestOldCompletionCallback client_connect_cb;
  DeleteOnConnected host_delete(&message_loop_, &host_pseudotcp_);
  net::OldCompletionCallbackImpl<DeleteOnConnected>
      host_connect_cb(&host_delete, &DeleteOnConnected::OnConnected);

  host_pseudotcp_->Connect(&host_connect_cb);
  client_pseudotcp_->Connect(&client_connect_cb);
  message_loop_.Run();

  ASSERT_EQ(NULL, host_pseudotcp_.get());
}

}  // namespace

}  // namespace jingle_glue
