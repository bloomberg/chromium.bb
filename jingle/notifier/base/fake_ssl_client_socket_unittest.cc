// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/fake_ssl_client_socket.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "net/base/capturing_net_log.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

using ::testing::Return;
using ::testing::ReturnRef;

// Used by RunUnsuccessfulHandshakeTestHelper.  Represents where in
// the handshake step an error should be inserted.
enum HandshakeErrorLocation {
  CONNECT_ERROR,
  SEND_CLIENT_HELLO_ERROR,
  VERIFY_SERVER_HELLO_ERROR,
};

// Private error codes appended to the net::Error set.
enum {
  // An error representing a server hello that has been corrupted in
  // transit.
  ERR_MALFORMED_SERVER_HELLO = -15000,
};

// Used by PassThroughMethods test.
class MockClientSocket : public net::StreamSocket {
 public:
  virtual ~MockClientSocket() {}

  MOCK_METHOD3(Read, int(net::IOBuffer*, int, net::OldCompletionCallback*));
  MOCK_METHOD3(Write, int(net::IOBuffer*, int, net::OldCompletionCallback*));
  MOCK_METHOD1(SetReceiveBufferSize, bool(int32));
  MOCK_METHOD1(SetSendBufferSize, bool(int32));
  MOCK_METHOD1(Connect, int(net::OldCompletionCallback*));
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnectedAndIdle, bool());
  MOCK_CONST_METHOD1(GetPeerAddress, int(net::AddressList*));
  MOCK_CONST_METHOD1(GetLocalAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD0(NetLog, const net::BoundNetLog&());
  MOCK_METHOD0(SetSubresourceSpeculation, void());
  MOCK_METHOD0(SetOmniboxSpeculation, void());
  MOCK_CONST_METHOD0(WasEverUsed, bool());
  MOCK_CONST_METHOD0(UsingTCPFastOpen, bool());
  MOCK_CONST_METHOD0(NumBytesRead, int64());
  MOCK_CONST_METHOD0(GetConnectTimeMicros, base::TimeDelta());
};

// Break up |data| into a bunch of chunked MockReads/Writes and push
// them onto |ops|.
void AddChunkedOps(base::StringPiece data, size_t chunk_size, bool async,
                   std::vector<net::MockRead>* ops) {
  DCHECK_GT(chunk_size, 0U);
  size_t offset = 0;
  while (offset < data.size()) {
    size_t bounded_chunk_size = std::min(data.size() - offset, chunk_size);
    // We take advantage of the fact that MockWrite is typedefed to
    // MockRead.
    ops->push_back(net::MockRead(async, data.data() + offset,
                                 bounded_chunk_size));
    offset += bounded_chunk_size;
  }
}

class FakeSSLClientSocketTest : public testing::Test {
 protected:
  FakeSSLClientSocketTest()
      : capturing_net_log_(net::CapturingNetLog::kUnbounded) {}

  virtual ~FakeSSLClientSocketTest() {}

  net::StreamSocket* MakeClientSocket() {
    return mock_client_socket_factory_.CreateTransportClientSocket(
        net::AddressList(), &capturing_net_log_, net::NetLog::Source());
  }

  void SetData(const net::MockConnect& mock_connect,
               std::vector<net::MockRead>* reads,
               std::vector<net::MockWrite>* writes) {
    static_socket_data_provider_.reset(
        new net::StaticSocketDataProvider(
            reads->empty() ? NULL : &*reads->begin(), reads->size(),
            writes->empty() ? NULL : &*writes->begin(), writes->size()));
    static_socket_data_provider_->set_connect_data(mock_connect);
    mock_client_socket_factory_.AddSocketDataProvider(
        static_socket_data_provider_.get());
  }

  void ExpectStatus(
      bool async, int expected_status, int immediate_status,
      TestOldCompletionCallback* test_completion_callback) {
    if (async) {
      EXPECT_EQ(net::ERR_IO_PENDING, immediate_status);
      int status = test_completion_callback->WaitForResult();
      EXPECT_EQ(expected_status, status);
    } else {
      EXPECT_EQ(expected_status, immediate_status);
    }
  }

  // Sets up the mock socket to generate a successful handshake
  // (sliced up according to the parameters) and makes sure the
  // FakeSSLClientSocket behaves as expected.
  void RunSuccessfulHandshakeTest(
      bool async, size_t read_chunk_size, size_t write_chunk_size,
      int num_resets) {
    base::StringPiece ssl_client_hello =
        FakeSSLClientSocket::GetSslClientHello();
    base::StringPiece ssl_server_hello =
        FakeSSLClientSocket::GetSslServerHello();

    net::MockConnect mock_connect(async, net::OK);
    std::vector<net::MockRead> reads;
    std::vector<net::MockWrite> writes;
    static const char kReadTestData[] = "read test data";
    static const char kWriteTestData[] = "write test data";
    for (int i = 0; i < num_resets + 1; ++i) {
      SCOPED_TRACE(i);
      AddChunkedOps(ssl_server_hello, read_chunk_size, async, &reads);
      AddChunkedOps(ssl_client_hello, write_chunk_size, async, &writes);
      reads.push_back(
          net::MockRead(async, kReadTestData, arraysize(kReadTestData)));
      writes.push_back(
          net::MockWrite(async, kWriteTestData, arraysize(kWriteTestData)));
    }
    SetData(mock_connect, &reads, &writes);

    FakeSSLClientSocket fake_ssl_client_socket(MakeClientSocket());

    for (int i = 0; i < num_resets + 1; ++i) {
      SCOPED_TRACE(i);
      TestOldCompletionCallback test_completion_callback;
      int status = fake_ssl_client_socket.Connect(&test_completion_callback);
      if (async) {
        EXPECT_FALSE(fake_ssl_client_socket.IsConnected());
      }
      ExpectStatus(async, net::OK, status, &test_completion_callback);
      if (fake_ssl_client_socket.IsConnected()) {
        int read_len = arraysize(kReadTestData);
        int read_buf_len = 2 * read_len;
        scoped_refptr<net::IOBuffer> read_buf(
            new net::IOBuffer(read_buf_len));
        int read_status = fake_ssl_client_socket.Read(
            read_buf, read_buf_len, &test_completion_callback);
        ExpectStatus(async, read_len, read_status, &test_completion_callback);

        scoped_refptr<net::IOBuffer> write_buf(
            new net::StringIOBuffer(kWriteTestData));
        int write_status = fake_ssl_client_socket.Write(
            write_buf, arraysize(kWriteTestData), &test_completion_callback);
        ExpectStatus(async, arraysize(kWriteTestData), write_status,
                     &test_completion_callback);
      } else {
        ADD_FAILURE();
      }
      fake_ssl_client_socket.Disconnect();
      EXPECT_FALSE(fake_ssl_client_socket.IsConnected());
    }
  }

  // Sets up the mock socket to generate an unsuccessful handshake
  // FakeSSLClientSocket fails as expected.
  void RunUnsuccessfulHandshakeTestHelper(
      bool async, int error, HandshakeErrorLocation location) {
    DCHECK_NE(error, net::OK);
    base::StringPiece ssl_client_hello =
        FakeSSLClientSocket::GetSslClientHello();
    base::StringPiece ssl_server_hello =
        FakeSSLClientSocket::GetSslServerHello();

    net::MockConnect mock_connect(async, net::OK);
    std::vector<net::MockRead> reads;
    std::vector<net::MockWrite> writes;
    const size_t kChunkSize = 1;
    AddChunkedOps(ssl_server_hello, kChunkSize, async, &reads);
    AddChunkedOps(ssl_client_hello, kChunkSize, async, &writes);
    switch (location) {
      case CONNECT_ERROR:
        mock_connect.result = error;
        writes.clear();
        reads.clear();
        break;
      case SEND_CLIENT_HELLO_ERROR: {
        // Use a fixed index for repeatability.
        size_t index = 100 % writes.size();
        writes[index].result = error;
        writes[index].data = NULL;
        writes[index].data_len = 0;
        writes.resize(index + 1);
        reads.clear();
        break;
      }
      case VERIFY_SERVER_HELLO_ERROR: {
        // Use a fixed index for repeatability.
        size_t index = 50 % reads.size();
        if (error == ERR_MALFORMED_SERVER_HELLO) {
          static const char kBadData[] = "BAD_DATA";
          reads[index].data = kBadData;
          reads[index].data_len = arraysize(kBadData);
        } else {
          reads[index].result = error;
          reads[index].data = NULL;
          reads[index].data_len = 0;
        }
        reads.resize(index + 1);
        if (error ==
            net::ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ) {
          static const char kDummyData[] = "DUMMY";
          reads.push_back(net::MockRead(async, kDummyData));
        }
        break;
      }
    }
    SetData(mock_connect, &reads, &writes);

    FakeSSLClientSocket fake_ssl_client_socket(MakeClientSocket());

    // The two errors below are interpreted by FakeSSLClientSocket as
    // an unexpected event.
    int expected_status =
        ((error == net::ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ) ||
         (error == ERR_MALFORMED_SERVER_HELLO)) ?
        net::ERR_UNEXPECTED : error;

    TestOldCompletionCallback test_completion_callback;
    int status = fake_ssl_client_socket.Connect(&test_completion_callback);
    EXPECT_FALSE(fake_ssl_client_socket.IsConnected());
    ExpectStatus(async, expected_status, status, &test_completion_callback);
    EXPECT_FALSE(fake_ssl_client_socket.IsConnected());
  }

  void RunUnsuccessfulHandshakeTest(
      int error, HandshakeErrorLocation location) {
    RunUnsuccessfulHandshakeTestHelper(false, error, location);
    RunUnsuccessfulHandshakeTestHelper(true, error, location);
  }

  // MockTCPClientSocket needs a message loop.
  MessageLoop message_loop_;

  net::CapturingNetLog capturing_net_log_;
  net::MockClientSocketFactory mock_client_socket_factory_;
  scoped_ptr<net::StaticSocketDataProvider> static_socket_data_provider_;
};

TEST_F(FakeSSLClientSocketTest, PassThroughMethods) {
  MockClientSocket* mock_client_socket = new MockClientSocket();
  const int kReceiveBufferSize = 10;
  const int kSendBufferSize = 20;
  net::AddressList address_list;
  const int kPeerAddress = 30;
  net::BoundNetLog net_log;
  EXPECT_CALL(*mock_client_socket, SetReceiveBufferSize(kReceiveBufferSize));
  EXPECT_CALL(*mock_client_socket, SetSendBufferSize(kSendBufferSize));
  EXPECT_CALL(*mock_client_socket, GetPeerAddress(&address_list)).
      WillOnce(Return(kPeerAddress));
  EXPECT_CALL(*mock_client_socket, NetLog()).WillOnce(ReturnRef(net_log));
  EXPECT_CALL(*mock_client_socket, SetSubresourceSpeculation());
  EXPECT_CALL(*mock_client_socket, SetOmniboxSpeculation());

  // Takes ownership of |mock_client_socket|.
  FakeSSLClientSocket fake_ssl_client_socket(mock_client_socket);
  fake_ssl_client_socket.SetReceiveBufferSize(kReceiveBufferSize);
  fake_ssl_client_socket.SetSendBufferSize(kSendBufferSize);
  EXPECT_EQ(kPeerAddress,
            fake_ssl_client_socket.GetPeerAddress(&address_list));
  EXPECT_EQ(&net_log, &fake_ssl_client_socket.NetLog());
  fake_ssl_client_socket.SetSubresourceSpeculation();
  fake_ssl_client_socket.SetOmniboxSpeculation();
}

TEST_F(FakeSSLClientSocketTest, SuccessfulHandshakeSync) {
  for (size_t i = 1; i < 100; i += 3) {
    SCOPED_TRACE(i);
    for (size_t j = 1; j < 100; j += 5) {
      SCOPED_TRACE(j);
      RunSuccessfulHandshakeTest(false, i, j, 0);
    }
  }
}

TEST_F(FakeSSLClientSocketTest, SuccessfulHandshakeAsync) {
  for (size_t i = 1; i < 100; i += 7) {
    SCOPED_TRACE(i);
    for (size_t j = 1; j < 100; j += 9) {
      SCOPED_TRACE(j);
      RunSuccessfulHandshakeTest(true, i, j, 0);
    }
  }
}

TEST_F(FakeSSLClientSocketTest, ResetSocket) {
  RunSuccessfulHandshakeTest(true, 1, 2, 3);
}

TEST_F(FakeSSLClientSocketTest, UnsuccessfulHandshakeConnectError) {
  RunUnsuccessfulHandshakeTest(net::ERR_ACCESS_DENIED, CONNECT_ERROR);
}

TEST_F(FakeSSLClientSocketTest, UnsuccessfulHandshakeWriteError) {
  RunUnsuccessfulHandshakeTest(net::ERR_OUT_OF_MEMORY,
                               SEND_CLIENT_HELLO_ERROR);
}

TEST_F(FakeSSLClientSocketTest, UnsuccessfulHandshakeReadError) {
  RunUnsuccessfulHandshakeTest(net::ERR_CONNECTION_CLOSED,
                               VERIFY_SERVER_HELLO_ERROR);
}

TEST_F(FakeSSLClientSocketTest, PeerClosedDuringHandshake) {
  RunUnsuccessfulHandshakeTest(
      net::ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ,
      VERIFY_SERVER_HELLO_ERROR);
}

TEST_F(FakeSSLClientSocketTest, MalformedServerHello) {
  RunUnsuccessfulHandshakeTest(ERR_MALFORMED_SERVER_HELLO,
                               VERIFY_SERVER_HELLO_ERROR);
}

}  // namespace

}  // namespace notifier
