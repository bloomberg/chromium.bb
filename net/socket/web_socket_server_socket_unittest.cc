// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/web_socket_server_socket.h"

#include <stdlib.h>
#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kSampleHandshakeRequest[] = {
    "GET /demo HTTP/1.1",
    "Upgrade: WebSocket",
    "Connection: Upgrade",
    "Host: example.com",
    "Origin: http://example.com",
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5",
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00",
    "",
    "^n:ds[4U"
};

const char kSampleHandshakeAnswer[] =
    "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Location: ws://example.com/demo\r\n"
    "Sec-WebSocket-Origin: http://example.com\r\n"
    "\r\n"
    "8jKS'y:G*Co,Wxa-";

const int kHandshakeBufBytes = 1 << 12;

const char kCRLF[] = "\r\n";
const char kCRLFCRLF[] = "\r\n\r\n";
const char kSpaceOctet = '\x20';

const int kReadSalt = 7;
const int kWriteSalt = 5;

int GetRand(int min, int max) {
  CHECK(max >= min);
  CHECK(max - min < RAND_MAX);
  return rand() % (max - min + 1) + min;
}

class RandIntClass {
 public:
  int operator() (int range) {
    return GetRand(0, range - 1);
  }
} g_rand;

net::DrainableIOBuffer* ResizeIOBuffer(net::DrainableIOBuffer* buf, int len) {
  net::DrainableIOBuffer* rv = new net::DrainableIOBuffer(
      new net::IOBuffer(len), len);
  std::copy(buf->data(), buf->data() + std::min(len, buf->BytesRemaining()),
            rv->data());
  return rv;
}

// TODO(dilmah): consider switching to socket_test_util.h
// Simulates reading from |sample| stream; data supplied in Write() calls are
// stored in |answer| buffer.
class TestingTransportSocket : public net::Socket {
 public:
  TestingTransportSocket(
      net::DrainableIOBuffer* sample, net::DrainableIOBuffer* answer)
      : sample_(sample),
        answer_(answer),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }

  ~TestingTransportSocket() {
    if (!final_read_callback_.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&TestingTransportSocket::DoReadCallback,
                     weak_factory_.GetWeakPtr(),
                     final_read_callback_, 0));
    }
  }

  // Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) {
    CHECK_GT(buf_len, 0);
    int remaining = sample_->BytesRemaining();
    if (remaining < 1) {
      if (!final_read_callback_.is_null())
        return 0;
      final_read_callback_ = callback;
      return net::ERR_IO_PENDING;
    }
    int lot = GetRand(1, std::min(remaining, buf_len));
    std::copy(sample_->data(), sample_->data() + lot, buf->data());
    sample_->DidConsume(lot);
    if (GetRand(0, 1)) {
      return lot;
    }
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestingTransportSocket::DoReadCallback,
                   weak_factory_.GetWeakPtr(), callback, lot));
    return net::ERR_IO_PENDING;
  }

  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) {
    CHECK_GT(buf_len, 0);
    int remaining = answer_->BytesRemaining();
    CHECK_GE(remaining, buf_len);
    int lot = std::min(remaining, buf_len);
    if (GetRand(0, 1))
      lot = GetRand(1, lot);
    std::copy(buf->data(), buf->data() + lot, answer_->data());
    answer_->DidConsume(lot);
    if (GetRand(0, 1)) {
      return lot;
    }
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestingTransportSocket::DoWriteCallback,
                   weak_factory_.GetWeakPtr(), callback, lot));
    return net::ERR_IO_PENDING;
  }

  virtual bool SetReceiveBufferSize(int32 size) {
    return true;
  }

  virtual bool SetSendBufferSize(int32 size) {
    return true;
  }

  net::DrainableIOBuffer* answer() { return answer_.get(); }

  void DoReadCallback(const net::CompletionCallback& callback, int result) {
    if (result == 0 && !is_closed_) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &TestingTransportSocket::DoReadCallback,
              weak_factory_.GetWeakPtr(), callback, 0));
    } else {
      if (!callback.is_null())
        callback.Run(result);
    }
  }

  void DoWriteCallback(const net::CompletionCallback& callback, int result) {
    if (!callback.is_null())
      callback.Run(result);
  }

  bool is_closed_;

  // Data to return for Read requests.
  scoped_refptr<net::DrainableIOBuffer> sample_;

  // Data pushed to us by server socket (using Write calls).
  scoped_refptr<net::DrainableIOBuffer> answer_;

  // Final read callback to report zero (zero stands for EOF).
  net::CompletionCallback final_read_callback_;

  base::WeakPtrFactory<TestingTransportSocket> weak_factory_;
};

class Validator : public net::WebSocketServerSocket::Delegate {
 public:
  Validator(const std::string& resource,
            const std::string& origin,
            const std::string& host)
      : resource_(resource), origin_(origin), host_(host) {
  }

  // WebSocketServerSocket::Delegate implementation.
  virtual bool ValidateWebSocket(
      const std::string& resource,
      const std::string& origin,
      const std::string& host,
      const std::vector<std::string>& subprotocol_list,
      std::string* location_out,
      std::string* subprotocol_out) {
    if (resource != resource_ || origin != origin_ || host != host_)
      return false;
    if (!subprotocol_list.empty())
      *subprotocol_out = subprotocol_list.front();

    char tmp[2048];
    base::snprintf(
        tmp, sizeof(tmp), "ws://%s%s", host.c_str(), resource.c_str());
    location_out->assign(tmp);
    return true;
  }

 private:
  std::string resource_;
  std::string origin_;
  std::string host_;
};

char ReferenceSeq(unsigned n, unsigned salt) {
  return (salt * 2 + n * 3) % ('z' - 'a') + 'a';
}

class ReadWriteTracker {
 public:
  ReadWriteTracker(
      net::WebSocketServerSocket* ws, int bytes_to_read, int bytes_to_write)
      : ws_(ws),
        buf_size_(1 << 14),
        read_buf_(new net::IOBuffer(buf_size_)),
        write_buf_(new net::IOBuffer(buf_size_)),
        bytes_remaining_to_read_(bytes_to_read),
        bytes_remaining_to_write_(bytes_to_write),
        got_final_zero_(false) {
    int rv = ws_->Accept(
        base::Bind(&ReadWriteTracker::OnAccept, base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING)
      OnAccept(rv);
  }

  ~ReadWriteTracker() {
    CHECK_EQ(bytes_remaining_to_write_, 0);
    CHECK_EQ(bytes_remaining_to_read_, 0);
  }

  void OnAccept(int result) {
    ASSERT_EQ(result, 0);
    if (GetRand(0, 1)) {
      DoRead();
      DoWrite();
    } else {
      DoWrite();
      DoRead();
    }
  }

  void DoWrite() {
    if (bytes_remaining_to_write_ < 1)
      return;
    int lot = GetRand(1, bytes_remaining_to_write_);
    lot = std::min(lot, buf_size_);
    for (int i = 0; i < lot; ++i)
      write_buf_->data()[i] = ReferenceSeq(
          bytes_remaining_to_write_ - i - 1, kWriteSalt);
    int rv = ws_->Write(write_buf_, lot, base::Bind(&ReadWriteTracker::OnWrite,
                                                    base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING)
      OnWrite(rv);
  }

  void DoRead() {
    int lot = GetRand(1, buf_size_);
    if (bytes_remaining_to_read_ < 1) {
      if (got_final_zero_)
        return;
    } else {
      lot = GetRand(1, bytes_remaining_to_read_);
      lot = std::min(lot, buf_size_);
    }
    int rv = ws_->Read(read_buf_, lot, base::Bind(&ReadWriteTracker::OnRead,
                                                  base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING)
      OnRead(rv);
  }

  void OnWrite(int result) {
    ASSERT_GT(result, 0);
    ASSERT_LE(result, bytes_remaining_to_write_);
    bytes_remaining_to_write_ -= result;
    DoWrite();
  }

  void OnRead(int result) {
    ASSERT_LE(result, bytes_remaining_to_read_);
    if (bytes_remaining_to_read_ < 1) {
      ASSERT_FALSE(got_final_zero_);
      ASSERT_EQ(result, 0);
      got_final_zero_ = true;
      return;
    }
    for (int i = 0; i < result; ++i) {
      ASSERT_EQ(read_buf_->data()[i], ReferenceSeq(
          bytes_remaining_to_read_ - i - 1, kReadSalt));
    }
    bytes_remaining_to_read_ -= result;
    DoRead();
  }

 private:
  net::WebSocketServerSocket* const ws_;
  int const buf_size_;
  scoped_refptr<net::IOBuffer> read_buf_;
  scoped_refptr<net::IOBuffer> write_buf_;
  int bytes_remaining_to_read_;
  int bytes_remaining_to_write_;
  bool got_final_zero_;
};

}  // namespace

namespace net {

class WebSocketServerSocketTest : public testing::Test {
 public:
  virtual ~WebSocketServerSocketTest() {
  }

  virtual void SetUp() {
    count_ = 0;
  }

  virtual void TearDown() {
  }

  void OnAccept0(int result) {
    ASSERT_EQ(result, 0);
    ASSERT_LT(count_, 99999);
    count_ += 1;
  }

  void OnAccept1(int result) {
    ASSERT_TRUE(result == ERR_CONNECTION_REFUSED ||
        result == ERR_ACCESS_DENIED);
    ASSERT_LT(count_, 99999);
    count_ += 1;
  }

  int count_;
};

TEST_F(WebSocketServerSocketTest, Handshake) {
  srand(2523456);
  std::vector<Socket*> kill_list;
  std::vector< scoped_refptr<DrainableIOBuffer> > answer_list;
  Validator validator("/demo", "http://example.com/", "example.com");
  count_ = 0;
  const int kNumTests = 300;
  for (int run = kNumTests; run--;) {
    scoped_refptr<DrainableIOBuffer> sample = new DrainableIOBuffer(
        new IOBuffer(kHandshakeBufBytes), kHandshakeBufBytes);
    for (size_t i = 0; i < arraysize(kSampleHandshakeRequest); ++i) {
      std::copy(kSampleHandshakeRequest[i],
                kSampleHandshakeRequest[i] + strlen(kSampleHandshakeRequest[i]),
                sample->data());
      sample->DidConsume(strlen(kSampleHandshakeRequest[i]));
      if (i != arraysize(kSampleHandshakeRequest) - 1) {
        std::copy(kCRLF, kCRLF + strlen(kCRLF), sample->data());
        sample->DidConsume(strlen(kCRLF));
      }
    }
    int sample_len = sample->BytesConsumed();
    sample->SetOffset(0);
    DrainableIOBuffer* answer = new DrainableIOBuffer(
        new IOBuffer(kHandshakeBufBytes), kHandshakeBufBytes);
    answer_list.push_back(answer);
    TestingTransportSocket* transport = new TestingTransportSocket(
        ResizeIOBuffer(sample.get(), sample_len), answer);
    WebSocketServerSocket* ws = CreateWebSocketServerSocket(
        transport, &validator);
    ASSERT_TRUE(ws != NULL);
    kill_list.push_back(ws);

    int rv = ws->Accept(base::Bind(&WebSocketServerSocketTest::OnAccept0,
                                   base::Unretained(this)));
    if (rv != ERR_IO_PENDING)
      OnAccept0(rv);
  }
  MessageLoop::current()->RunAllPending();
  ASSERT_EQ(count_, kNumTests);
  for (size_t i = answer_list.size(); i--;) {
    ASSERT_EQ(answer_list[i]->BytesConsumed() + 0u,
              strlen(kSampleHandshakeAnswer));
    ASSERT_TRUE(std::equal(
        answer_list[i]->data() - answer_list[i]->BytesConsumed(),
        answer_list[i]->data(), kSampleHandshakeAnswer));
  }
  for (size_t i = kill_list.size(); i--;)
    delete kill_list[i];
  MessageLoop::current()->RunAllPending();
}

TEST_F(WebSocketServerSocketTest, BadCred) {
  srand(9034958);
  std::vector<Socket*> kill_list;
  std::vector< scoped_refptr<DrainableIOBuffer> > answer_list;
  Validator *validator[] = {
      new Validator("/demo", "http://gooogle.com/", "example.com"),
      new Validator("/tcpproxy", "http://example.com/", "example.com"),
      new Validator("/tcpproxy", "http://gooogle.com/", "example.com"),
      new Validator("/demo", "http://example.com/", "exmple.com"),
      new Validator("/demo", "http://gooogle.com/", "gooogle.com")
  };
  count_ = 0;
  for (int run = arraysize(validator); run--;) {
    scoped_refptr<DrainableIOBuffer> sample = new DrainableIOBuffer(
        new IOBuffer(kHandshakeBufBytes), kHandshakeBufBytes);
    for (size_t i = 0; i < arraysize(kSampleHandshakeRequest); ++i) {
      std::copy(kSampleHandshakeRequest[i],
                kSampleHandshakeRequest[i] + strlen(kSampleHandshakeRequest[i]),
                sample->data());
      sample->DidConsume(strlen(kSampleHandshakeRequest[i]));
      if (i != arraysize(kSampleHandshakeRequest) - 1) {
        std::copy(kCRLF, kCRLF + strlen(kCRLF), sample->data());
        sample->DidConsume(strlen(kCRLF));
      }
    }
    int sample_len = sample->BytesConsumed();
    sample->SetOffset(0);
    DrainableIOBuffer* answer = new DrainableIOBuffer(
        new IOBuffer(kHandshakeBufBytes), kHandshakeBufBytes);
    answer_list.push_back(answer);
    TestingTransportSocket* transport = new TestingTransportSocket(
        ResizeIOBuffer(sample.get(), sample_len), answer);
    WebSocketServerSocket* ws = CreateWebSocketServerSocket(
        transport, validator[run]);
    ASSERT_TRUE(ws != NULL);
    kill_list.push_back(ws);

    int rv = ws->Accept(base::Bind(&WebSocketServerSocketTest::OnAccept1,
                                   base::Unretained(this)));
    if (rv != ERR_IO_PENDING)
      OnAccept1(rv);
  }
  MessageLoop::current()->RunAllPending();
  ASSERT_EQ(count_ + 0u, arraysize(validator));
  for (size_t i = answer_list.size(); i--;)
    ASSERT_EQ(answer_list[i]->BytesConsumed(), 0);
  for (size_t i = kill_list.size(); i--;)
    delete kill_list[i];
  for (size_t i = arraysize(validator); i--;)
    delete validator[i];
  MessageLoop::current()->RunAllPending();
}

TEST_F(WebSocketServerSocketTest, ReorderedHandshake) {
  srand(205643459);
  std::vector<Socket*> kill_list;
  std::vector< scoped_refptr<DrainableIOBuffer> > answer_list;
  Validator validator("/demo", "http://example.com/", "example.com");
  count_ = 0;
  const int kNumTests = 200;
  for (int run = kNumTests; run--;) {
    scoped_refptr<DrainableIOBuffer> sample = new DrainableIOBuffer(
        new IOBuffer(kHandshakeBufBytes), kHandshakeBufBytes);

    std::vector<size_t> fields_order;
    for (size_t i = 0; i < arraysize(kSampleHandshakeRequest); ++i)
      fields_order.push_back(i);
    // One leading and two trailing lines of request are special, leave them.
    std::random_shuffle(fields_order.begin() + 1,
                        fields_order.begin() + fields_order.size() - 3,
                        g_rand);

    for (size_t i = 0; i < arraysize(kSampleHandshakeRequest); ++i) {
      size_t j = fields_order[i];
      std::copy(kSampleHandshakeRequest[j],
                kSampleHandshakeRequest[j] + strlen(kSampleHandshakeRequest[j]),
                sample->data());
      sample->DidConsume(strlen(kSampleHandshakeRequest[j]));
      if (i != arraysize(kSampleHandshakeRequest) - 1) {
        std::copy(kCRLF, kCRLF + strlen(kCRLF), sample->data());
        sample->DidConsume(strlen(kCRLF));
      }
    }
    int sample_len = sample->BytesConsumed();
    sample->SetOffset(0);
    DrainableIOBuffer* answer = new DrainableIOBuffer(
        new IOBuffer(kHandshakeBufBytes), kHandshakeBufBytes);
    answer_list.push_back(answer);
    TestingTransportSocket* transport = new TestingTransportSocket(
        ResizeIOBuffer(sample.get(), sample_len), answer);
    WebSocketServerSocket* ws = CreateWebSocketServerSocket(
        transport, &validator);
    ASSERT_TRUE(ws != NULL);
    kill_list.push_back(ws);

    int rv = ws->Accept(base::Bind(&WebSocketServerSocketTest::OnAccept0,
                                   base::Unretained(this)));
    if (rv != ERR_IO_PENDING)
      OnAccept0(rv);
  }
  MessageLoop::current()->RunAllPending();
  ASSERT_EQ(count_, kNumTests);
  for (size_t i = answer_list.size(); i--;) {
    ASSERT_EQ(answer_list[i]->BytesConsumed() + 0u,
              strlen(kSampleHandshakeAnswer));
    ASSERT_TRUE(std::equal(
        answer_list[i]->data() - answer_list[i]->BytesConsumed(),
        answer_list[i]->data(), kSampleHandshakeAnswer));
  }
  for (size_t i = kill_list.size(); i--;)
    delete kill_list[i];
  MessageLoop::current()->RunAllPending();
}

TEST_F(WebSocketServerSocketTest, ConveyData) {
  srand(8234523);
  std::vector<Socket*> kill_list;
  std::vector<ReadWriteTracker*> tracker_list;
  Validator validator("/demo", "http://example.com/", "example.com");
  count_ = 0;
  const int kNumTests = 150;
  for (int run = kNumTests; run--;) {
    int bytes_to_read = GetRand(1, 1 << 14);
    int bytes_to_write = GetRand(1, 1 << 14);
    int frames_limit = GetRand(1, 1 << 10);
    int sample_limit = kHandshakeBufBytes + bytes_to_write + frames_limit * 2;
    scoped_refptr<DrainableIOBuffer> sample = new DrainableIOBuffer(
        new IOBuffer(sample_limit), sample_limit);

    std::vector<size_t> fields_order;
    for (size_t i = 0; i < arraysize(kSampleHandshakeRequest); ++i)
      fields_order.push_back(i);
    // One leading and two trailing lines of request are special, leave them.
    std::random_shuffle(fields_order.begin() + 1,
                        fields_order.begin() + fields_order.size() - 3,
                        g_rand);

    for (size_t i = 0; i < arraysize(kSampleHandshakeRequest); ++i) {
      size_t j = fields_order[i];
      std::copy(kSampleHandshakeRequest[j],
                kSampleHandshakeRequest[j] + strlen(kSampleHandshakeRequest[j]),
                sample->data());
      sample->DidConsume(strlen(kSampleHandshakeRequest[j]));
      if (i != arraysize(kSampleHandshakeRequest) - 1) {
        std::copy(kCRLF, kCRLF + strlen(kCRLF), sample->data());
        sample->DidConsume(strlen(kCRLF));
      }
    }
    {
      bool outside_frame = true;
      int pos = 0;
      for (int i = 0; i < bytes_to_write; ++i) {
        if (outside_frame) {
          sample->data()[pos++] = '\x00';
          outside_frame = false;
          CHECK_GE(frames_limit, 1);
          frames_limit -= 1;
        }
        sample->data()[pos++] = ReferenceSeq(bytes_to_write - i - 1, kReadSalt);
        if ((frames_limit > 1 &&
             GetRand(0, 1 + (bytes_to_write - i) / frames_limit) == 0) ||
            i == bytes_to_write - 1) {
            sample->data()[pos++] = '\xff';
            outside_frame = true;
        }
      }
      sample->DidConsume(pos);
    }

    int sample_len = sample->BytesConsumed();
    sample->SetOffset(0);
    int answer_limit = kHandshakeBufBytes + bytes_to_read * 3;
    DrainableIOBuffer* answer = new DrainableIOBuffer(
        new IOBuffer(answer_limit), answer_limit);
    TestingTransportSocket* transport = new TestingTransportSocket(
        ResizeIOBuffer(sample.get(), sample_len), answer);
    WebSocketServerSocket* ws = CreateWebSocketServerSocket(
        transport, &validator);
    ASSERT_TRUE(ws != NULL);
    kill_list.push_back(ws);

    ReadWriteTracker* tracker = new ReadWriteTracker(
        ws, bytes_to_write, bytes_to_read);
    tracker_list.push_back(tracker);
  }
  MessageLoop::current()->RunAllPending();

  for (size_t i = kill_list.size(); i--;)
    delete kill_list[i];
  for (size_t i = tracker_list.size(); i--;)
    delete tracker_list[i];
  MessageLoop::current()->RunAllPending();
}

}  // namespace net
