// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_TEST_UTIL_H_
#define NET_SOCKET_SOCKET_TEST_UTIL_H_

#include <stdint.h>

#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service.h"
#include "net/udp/datagram_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

enum {
  // A private network error code used by the socket test utility classes.
  // If the |result| member of a MockRead is
  // ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ, that MockRead is just a
  // marker that indicates the peer will close the connection after the next
  // MockRead.  The other members of that MockRead are ignored.
  ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ = -10000,
};

class AsyncSocket;
class ChannelIDService;
class MockClientSocket;
class SSLClientSocket;
class StreamSocket;

enum IoMode {
  ASYNC,
  SYNCHRONOUS
};

struct MockConnect {
  // Asynchronous connection success.
  // Creates a MockConnect with |mode| ASYC, |result| OK, and
  // |peer_addr| 192.0.2.33.
  MockConnect();
  // Creates a MockConnect with the specified mode and result, with
  // |peer_addr| 192.0.2.33.
  MockConnect(IoMode io_mode, int r);
  MockConnect(IoMode io_mode, int r, IPEndPoint addr);
  ~MockConnect();

  IoMode mode;
  int result;
  IPEndPoint peer_addr;
};

// MockRead and MockWrite shares the same interface and members, but we'd like
// to have distinct types because we don't want to have them used
// interchangably. To do this, a struct template is defined, and MockRead and
// MockWrite are instantiated by using this template. Template parameter |type|
// is not used in the struct definition (it purely exists for creating a new
// type).
//
// |data| in MockRead and MockWrite has different meanings: |data| in MockRead
// is the data returned from the socket when MockTCPClientSocket::Read() is
// attempted, while |data| in MockWrite is the expected data that should be
// given in MockTCPClientSocket::Write().
enum MockReadWriteType {
  MOCK_READ,
  MOCK_WRITE
};

template <MockReadWriteType type>
struct MockReadWrite {
  // Flag to indicate that the message loop should be terminated.
  enum {
    STOPLOOP = 1 << 31
  };

  // Default
  MockReadWrite()
      : mode(SYNCHRONOUS),
        result(0),
        data(NULL),
        data_len(0),
        sequence_number(0) {}

  // Read/write failure (no data).
  MockReadWrite(IoMode io_mode, int result)
      : mode(io_mode),
        result(result),
        data(NULL),
        data_len(0),
        sequence_number(0) {}

  // Read/write failure (no data), with sequence information.
  MockReadWrite(IoMode io_mode, int result, int seq)
      : mode(io_mode),
        result(result),
        data(NULL),
        data_len(0),
        sequence_number(seq) {}

  // Asynchronous read/write success (inferred data length).
  explicit MockReadWrite(const char* data)
      : mode(ASYNC),
        result(0),
        data(data),
        data_len(strlen(data)),
        sequence_number(0) {}

  // Read/write success (inferred data length).
  MockReadWrite(IoMode io_mode, const char* data)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(strlen(data)),
        sequence_number(0) {}

  // Read/write success.
  MockReadWrite(IoMode io_mode, const char* data, int data_len)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(data_len),
        sequence_number(0) {}

  // Read/write success (inferred data length) with sequence information.
  MockReadWrite(IoMode io_mode, int seq, const char* data)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(strlen(data)),
        sequence_number(seq) {}

  // Read/write success with sequence information.
  MockReadWrite(IoMode io_mode, const char* data, int data_len, int seq)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(data_len),
        sequence_number(seq) {}

  IoMode mode;
  int result;
  const char* data;
  int data_len;

  // For data providers that only allows reads to occur in a particular
  // sequence.  If a read occurs before the given |sequence_number| is reached,
  // an ERR_IO_PENDING is returned.
  int sequence_number;    // The sequence number at which a read is allowed
                          // to occur.
};

typedef MockReadWrite<MOCK_READ> MockRead;
typedef MockReadWrite<MOCK_WRITE> MockWrite;

struct MockWriteResult {
  MockWriteResult(IoMode io_mode, int result) : mode(io_mode), result(result) {}

  IoMode mode;
  int result;
};

// The SocketDataProvider is an interface used by the MockClientSocket
// for getting data about individual reads and writes on the socket.
class SocketDataProvider {
 public:
  SocketDataProvider() : socket_(NULL) {}

  virtual ~SocketDataProvider() {}

  // Returns the buffer and result code for the next simulated read.
  // If the |MockRead.result| is ERR_IO_PENDING, it informs the caller
  // that it will be called via the AsyncSocket::OnReadComplete()
  // function at a later time.
  virtual MockRead OnRead() = 0;
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual void Reset() = 0;
  virtual bool AllReadDataConsumed() const = 0;
  virtual bool AllWriteDataConsumed() const = 0;

  // Accessor for the socket which is using the SocketDataProvider.
  AsyncSocket* socket() { return socket_; }
  void set_socket(AsyncSocket* socket) { socket_ = socket; }

  MockConnect connect_data() const { return connect_; }
  void set_connect_data(const MockConnect& connect) { connect_ = connect; }

 private:
  MockConnect connect_;
  AsyncSocket* socket_;

  DISALLOW_COPY_AND_ASSIGN(SocketDataProvider);
};

// The AsyncSocket is an interface used by the SocketDataProvider to
// complete the asynchronous read operation.
class AsyncSocket {
 public:
  // If an async IO is pending because the SocketDataProvider returned
  // ERR_IO_PENDING, then the AsyncSocket waits until this OnReadComplete
  // is called to complete the asynchronous read operation.
  // data.async is ignored, and this read is completed synchronously as
  // part of this call.
  // TODO(rch): this should take a StringPiece since most of the fields
  // are ignored.
  virtual void OnReadComplete(const MockRead& data) = 0;
  // If an async IO is pending because the SocketDataProvider returned
  // ERR_IO_PENDING, then the AsyncSocket waits until this OnReadComplete
  // is called to complete the asynchronous read operation.
  virtual void OnWriteComplete(int rv) = 0;
  virtual void OnConnectComplete(const MockConnect& data) = 0;
};

// StaticSocketDataHelper manages a list of reads and writes.
class StaticSocketDataHelper {
 public:
  StaticSocketDataHelper(MockRead* reads,
                         size_t reads_count,
                         MockWrite* writes,
                         size_t writes_count);
  ~StaticSocketDataHelper();

  // These functions get access to the next available read and write data,
  // or null if there is no more data available.
  const MockRead& PeekRead() const;
  const MockWrite& PeekWrite() const;

  // Returns the current read or write , and then advances to the next one.
  const MockRead& AdvanceRead();
  const MockWrite& AdvanceWrite();

  // Resets the read and write indexes to 0.
  void Reset();

  // Returns true if |data| is valid data for the next write. In order
  // to support short writes, the next write may be longer than |data|
  // in which case this method will still return true.
  bool VerifyWriteData(const std::string& data);

  size_t read_index() const { return read_index_; }
  size_t write_index() const { return write_index_; }
  size_t read_count() const { return read_count_; }
  size_t write_count() const { return write_count_; }

  bool AllReadDataConsumed() const { return read_index_ >= read_count_; }
  bool AllWriteDataConsumed() const { return write_index_ >= write_count_; }

 private:
  MockRead* reads_;
  size_t read_index_;
  size_t read_count_;
  MockWrite* writes_;
  size_t write_index_;
  size_t write_count_;

  DISALLOW_COPY_AND_ASSIGN(StaticSocketDataHelper);
};

// SocketDataProvider which responds based on static tables of mock reads and
// writes.
class StaticSocketDataProvider : public SocketDataProvider {
 public:
  StaticSocketDataProvider();
  StaticSocketDataProvider(MockRead* reads,
                           size_t reads_count,
                           MockWrite* writes,
                           size_t writes_count);
  ~StaticSocketDataProvider() override;

  virtual void CompleteRead() {}

  // SocketDataProvider implementation.
  MockRead OnRead() override;
  MockWriteResult OnWrite(const std::string& data) override;
  void Reset() override;
  bool AllReadDataConsumed() const override;
  bool AllWriteDataConsumed() const override;

  size_t read_index() const { return helper_.read_index(); }
  size_t write_index() const { return helper_.write_index(); }
  size_t read_count() const { return helper_.read_count(); }
  size_t write_count() const { return helper_.write_count(); }

 protected:
  StaticSocketDataHelper* helper() { return &helper_; }

 private:
  StaticSocketDataHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(StaticSocketDataProvider);
};

// SSLSocketDataProviders only need to keep track of the return code from calls
// to Connect().
struct SSLSocketDataProvider {
  SSLSocketDataProvider(IoMode mode, int result);
  ~SSLSocketDataProvider();

  void SetNextProto(NextProto proto);

  MockConnect connect;
  SSLClientSocket::NextProtoStatus next_proto_status;
  std::string next_proto;
  NextProtoVector next_protos_expected_in_ssl_config;
  bool client_cert_sent;
  SSLCertRequestInfo* cert_request_info;
  scoped_refptr<X509Certificate> cert;
  bool channel_id_sent;
  ChannelIDService* channel_id_service;
  int connection_status;
};

// Uses the sequence_number field in the mock reads and writes to
// complete the operations in a specified order.
class SequencedSocketData : public SocketDataProvider {
 public:
  // |reads| is the list of MockRead completions.
  // |writes| is the list of MockWrite completions.
  SequencedSocketData(MockRead* reads,
                      size_t reads_count,
                      MockWrite* writes,
                      size_t writes_count);

  // |connect| is the result for the connect phase.
  // |reads| is the list of MockRead completions.
  // |writes| is the list of MockWrite completions.
  SequencedSocketData(const MockConnect& connect,
                      MockRead* reads,
                      size_t reads_count,
                      MockWrite* writes,
                      size_t writes_count);

  ~SequencedSocketData() override;

  // SocketDataProviderBase implementation.
  MockRead OnRead() override;
  MockWriteResult OnWrite(const std::string& data) override;
  void Reset() override;
  bool AllReadDataConsumed() const override;
  bool AllWriteDataConsumed() const override;

  bool IsReadPaused();
  void CompleteRead();

 private:
  // Defines the state for the read or write path.
  enum IoState {
    IDLE,        // No async operation is in progress.
    PENDING,     // An async operation in waiting for another opteration to
                 // complete.
    COMPLETING,  // A task has been posted to complet an async operation.
    PAUSED,      // IO is paused until CompleteRead() is called.
  };

  void OnReadComplete();
  void OnWriteComplete();

  void MaybePostReadCompleteTask();
  void MaybePostWriteCompleteTask();

  StaticSocketDataHelper helper_;
  int sequence_number_;
  IoState read_state_;
  IoState write_state_;

  base::WeakPtrFactory<SequencedSocketData> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SequencedSocketData);
};

class DeterministicMockTCPClientSocket;

// This class gives the user full control over the network activity,
// specifically the timing of the COMPLETION of I/O operations.  Regardless of
// the order in which I/O operations are initiated, this class ensures that they
// complete in the correct order.
//
// Network activity is modeled as a sequence of numbered steps which is
// incremented whenever an I/O operation completes.  This can happen under two
// different circumstances:
//
// 1) Performing a synchronous I/O operation.  (Invoking Read() or Write()
//    when the corresponding MockRead or MockWrite is marked !async).
// 2) Running the Run() method of this class.  The run method will invoke
//    the current MessageLoop, running all pending events, and will then
//    invoke any pending IO callbacks.
//
// In addition, this class allows for I/O processing to "stop" at a specified
// step, by calling SetStop(int) or StopAfter(int).  Initiating an I/O operation
// by calling Read() or Write() while stopped is permitted if the operation is
// asynchronous.  It is an error to perform synchronous I/O while stopped.
//
// When creating the MockReads and MockWrites, note that the sequence number
// refers to the number of the step in which the I/O will complete.  In the
// case of synchronous I/O, this will be the same step as the I/O is initiated.
// However, in the case of asynchronous I/O, this I/O may be initiated in
// a much earlier step. Furthermore, when the a Read() or Write() is separated
// from its completion by other Read() or Writes()'s, it can not be marked
// synchronous.  If it is, ERR_UNUEXPECTED will be returned indicating that a
// synchronous Read() or Write() could not be completed synchronously because of
// the specific ordering constraints.
//
// Sequence numbers are preserved across both reads and writes. There should be
// no gaps in sequence numbers, and no repeated sequence numbers. i.e.
//  MockRead reads[] = {
//    MockRead(false, "first read", length, 0)   // sync
//    MockRead(true, "second read", length, 2)   // async
//  };
//  MockWrite writes[] = {
//    MockWrite(true, "first write", length, 1),    // async
//    MockWrite(false, "second write", length, 3),  // sync
//  };
//
// Example control flow:
// Read() is called.  The current step is 0.  The first available read is
// synchronous, so the call to Read() returns length.  The current step is
// now 1.  Next, Read() is called again.  The next available read can
// not be completed until step 2, so Read() returns ERR_IO_PENDING.  The current
// step is still 1.  Write is called().  The first available write is able to
// complete in this step, but is marked asynchronous.  Write() returns
// ERR_IO_PENDING.  The current step is still 1.  At this point RunFor(1) is
// called which will cause the write callback to be invoked, and will then
// stop.  The current state is now 2.  RunFor(1) is called again, which
// causes the read callback to be invoked, and will then stop.  Then current
// step is 2.  Write() is called again.  Then next available write is
// synchronous so the call to Write() returns length.
//
// For examples of how to use this class, see:
//   deterministic_socket_data_unittests.cc
class DeterministicSocketData {
 public:
  // The Delegate is an abstract interface which handles the communication from
  // the DeterministicSocketData to the Deterministic MockSocket.  The
  // MockSockets directly store a pointer to the DeterministicSocketData,
  // whereas the DeterministicSocketData only stores a pointer to the
  // abstract Delegate interface.
  class Delegate {
   public:
    // Returns true if there is currently a write pending. That is to say, if
    // an asynchronous write has been started but the callback has not been
    // invoked.
    virtual bool WritePending() const = 0;
    // Returns true if there is currently a read pending. That is to say, if
    // an asynchronous read has been started but the callback has not been
    // invoked.
    virtual bool ReadPending() const = 0;
    // Called to complete an asynchronous write to execute the write callback.
    virtual void CompleteWrite() = 0;
    // Called to complete an asynchronous read to execute the read callback.
    virtual int CompleteRead() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  DeterministicSocketData(MockRead* reads,
                          size_t reads_count,
                          MockWrite* writes,
                          size_t writes_count);
  ~DeterministicSocketData();

  // Consume all the data up to the give stop point (via SetStop()).
  void Run();

  // Set the stop point to be |steps| from now, and then invoke Run().
  void RunFor(int steps);

  // Stop at step |seq|, which must be in the future.
  void SetStop(int seq);

  // Stop |seq| steps after the current step.
  void StopAfter(int seq);

  bool stopped() const { return stopped_; }
  void SetStopped(bool val) { stopped_ = val; }
  MockRead& current_read() { return current_read_; }
  MockWrite& current_write() { return current_write_; }
  int sequence_number() const { return sequence_number_; }
  void set_delegate(base::WeakPtr<Delegate> delegate) { delegate_ = delegate; }
  MockConnect connect_data() const { return connect_; }
  void set_connect_data(const MockConnect& connect) { connect_ = connect; }

  // When the socket calls Read(), that calls OnRead(), and expects either
  // ERR_IO_PENDING or data.
  MockRead OnRead();

  // When the socket calls Write(), it always completes synchronously. OnWrite()
  // checks to make sure the written data matches the expected data. The
  // callback will not be invoked until its sequence number is reached.
  MockWriteResult OnWrite(const std::string& data);

  bool AllReadDataConsumed() const;
  bool AllWriteDataConsumed() const;

 private:
  // Invoke the read and write callbacks, if the timing is appropriate.
  void InvokeCallbacks();

  void NextStep();

  void VerifyCorrectSequenceNumbers(MockRead* reads,
                                    size_t reads_count,
                                    MockWrite* writes,
                                    size_t writes_count);
  StaticSocketDataHelper helper_;
  MockConnect connect_;
  int sequence_number_;
  MockRead current_read_;
  MockWrite current_write_;
  int stopping_sequence_number_;
  bool stopped_;
  base::WeakPtr<Delegate> delegate_;
  bool print_debug_;
  bool is_running_;
};

// Holds an array of SocketDataProvider elements.  As Mock{TCP,SSL}StreamSocket
// objects get instantiated, they take their data from the i'th element of this
// array.
template <typename T>
class SocketDataProviderArray {
 public:
  SocketDataProviderArray() : next_index_(0) {}

  T* GetNext() {
    DCHECK_LT(next_index_, data_providers_.size());
    return data_providers_[next_index_++];
  }

  void Add(T* data_provider) {
    DCHECK(data_provider);
    data_providers_.push_back(data_provider);
  }

  size_t next_index() { return next_index_; }

  void ResetNextIndex() { next_index_ = 0; }

 private:
  // Index of the next |data_providers_| element to use. Not an iterator
  // because those are invalidated on vector reallocation.
  size_t next_index_;

  // SocketDataProviders to be returned.
  std::vector<T*> data_providers_;
};

class MockUDPClientSocket;
class MockTCPClientSocket;
class MockSSLClientSocket;

// ClientSocketFactory which contains arrays of sockets of each type.
// You should first fill the arrays using AddMock{SSL,}Socket. When the factory
// is asked to create a socket, it takes next entry from appropriate array.
// You can use ResetNextMockIndexes to reset that next entry index for all mock
// socket types.
class MockClientSocketFactory : public ClientSocketFactory {
 public:
  MockClientSocketFactory();
  ~MockClientSocketFactory() override;

  void AddSocketDataProvider(SocketDataProvider* socket);
  void AddSSLSocketDataProvider(SSLSocketDataProvider* socket);
  void ResetNextMockIndexes();

  SocketDataProviderArray<SocketDataProvider>& mock_data() {
    return mock_data_;
  }

  // ClientSocketFactory
  scoped_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) override;
  scoped_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* net_log,
      const NetLog::Source& source) override;
  scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<ClientSocketHandle> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      const SSLClientSocketContext& context) override;
  void ClearSSLSessionCache() override;

 private:
  SocketDataProviderArray<SocketDataProvider> mock_data_;
  SocketDataProviderArray<SSLSocketDataProvider> mock_ssl_data_;
};

class MockClientSocket : public SSLClientSocket {
 public:
  // Value returned by GetTLSUniqueChannelBinding().
  static const char kTlsUnique[];

  // The BoundNetLog is needed to test LoadTimingInfo, which uses NetLog IDs as
  // unique socket IDs.
  explicit MockClientSocket(const BoundNetLog& net_log);

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override = 0;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override = 0;
  int SetReceiveBufferSize(int32 size) override;
  int SetSendBufferSize(int32 size) override;

  // StreamSocket implementation.
  int Connect(const CompletionCallback& callback) override = 0;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override {}
  void SetOmniboxSpeculation() override {}
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}
  int64_t GetTotalReceivedBytes() const override;

  // SSLClientSocket implementation.
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override;
  int ExportKeyingMaterial(const base::StringPiece& label,
                           bool has_context,
                           const base::StringPiece& context,
                           unsigned char* out,
                           unsigned int outlen) override;
  int GetTLSUniqueChannelBinding(std::string* out) override;
  NextProtoStatus GetNextProto(std::string* proto) const override;
  ChannelIDService* GetChannelIDService() const override;
  SSLFailureState GetSSLFailureState() const override;

 protected:
  ~MockClientSocket() override;
  void RunCallbackAsync(const CompletionCallback& callback, int result);
  void RunCallback(const CompletionCallback& callback, int result);

  // True if Connect completed successfully and Disconnect hasn't been called.
  bool connected_;

  // Address of the "remote" peer we're connected to.
  IPEndPoint peer_addr_;

  BoundNetLog net_log_;

 private:
  base::WeakPtrFactory<MockClientSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocket);
};

class MockTCPClientSocket : public MockClientSocket, public AsyncSocket {
 public:
  MockTCPClientSocket(const AddressList& addresses,
                      net::NetLog* net_log,
                      SocketDataProvider* socket);
  ~MockTCPClientSocket() override;

  const AddressList& addresses() const { return addresses_; }

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;

  // StreamSocket implementation.
  int Connect(const CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  bool WasNpnNegotiated() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override;
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override;

  // AsyncSocket:
  void OnReadComplete(const MockRead& data) override;
  void OnWriteComplete(int rv) override;
  void OnConnectComplete(const MockConnect& data) override;

 private:
  int CompleteRead();

  AddressList addresses_;

  SocketDataProvider* data_;
  int read_offset_;
  MockRead read_data_;
  bool need_read_data_;

  // True if the peer has closed the connection.  This allows us to simulate
  // the recv(..., MSG_PEEK) call in the IsConnectedAndIdle method of the real
  // TCPClientSocket.
  bool peer_closed_connection_;

  // While an asynchronous read is pending, we save our user-buffer state.
  scoped_refptr<IOBuffer> pending_read_buf_;
  int pending_read_buf_len_;
  CompletionCallback pending_connect_callback_;
  CompletionCallback pending_read_callback_;
  CompletionCallback pending_write_callback_;
  bool was_used_to_convey_data_;

  ConnectionAttempts connection_attempts_;

  DISALLOW_COPY_AND_ASSIGN(MockTCPClientSocket);
};

// DeterministicSocketHelper is a helper class that can be used
// to simulate Socket::Read() and Socket::Write()
// using deterministic |data|.
// Note: This is provided as a common helper class because
// of the inheritance hierarchy of DeterministicMock[UDP,TCP]ClientSocket and a
// desire not to introduce an additional common base class.
class DeterministicSocketHelper {
 public:
  DeterministicSocketHelper(NetLog* net_log, DeterministicSocketData* data);
  virtual ~DeterministicSocketHelper();

  bool write_pending() const { return write_pending_; }
  bool read_pending() const { return read_pending_; }

  void CompleteWrite();
  int CompleteRead();

  int Write(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);

  const BoundNetLog& net_log() const { return net_log_; }

  bool was_used_to_convey_data() const { return was_used_to_convey_data_; }

  bool peer_closed_connection() const { return peer_closed_connection_; }

  DeterministicSocketData* data() const { return data_; }

 private:
  bool write_pending_;
  CompletionCallback write_callback_;
  int write_result_;

  MockRead read_data_;

  IOBuffer* read_buf_;
  int read_buf_len_;
  bool read_pending_;
  CompletionCallback read_callback_;
  DeterministicSocketData* data_;
  bool was_used_to_convey_data_;
  bool peer_closed_connection_;
  BoundNetLog net_log_;
};

// Mock UDP socket to be used in conjunction with DeterministicSocketData.
class DeterministicMockUDPClientSocket
    : public DatagramClientSocket,
      public DeterministicSocketData::Delegate,
      public base::SupportsWeakPtr<DeterministicMockUDPClientSocket> {
 public:
  DeterministicMockUDPClientSocket(net::NetLog* net_log,
                                   DeterministicSocketData* data);
  ~DeterministicMockUDPClientSocket() override;

  // DeterministicSocketData::Delegate:
  bool WritePending() const override;
  bool ReadPending() const override;
  void CompleteWrite() override;
  int CompleteRead() override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32 size) override;
  int SetSendBufferSize(int32 size) override;

  // DatagramSocket implementation.
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const BoundNetLog& NetLog() const override;

  // DatagramClientSocket implementation.
  int BindToNetwork(NetworkChangeNotifier::NetworkHandle network) override;
  int Connect(const IPEndPoint& address) override;

  void set_source_port(uint16 port) { source_port_ = port; }

 private:
  bool connected_;
  IPEndPoint peer_address_;
  DeterministicSocketHelper helper_;
  uint16 source_port_;  // Ephemeral source port.

  DISALLOW_COPY_AND_ASSIGN(DeterministicMockUDPClientSocket);
};

// Mock TCP socket to be used in conjunction with DeterministicSocketData.
class DeterministicMockTCPClientSocket
    : public MockClientSocket,
      public DeterministicSocketData::Delegate,
      public base::SupportsWeakPtr<DeterministicMockTCPClientSocket> {
 public:
  DeterministicMockTCPClientSocket(net::NetLog* net_log,
                                   DeterministicSocketData* data);
  ~DeterministicMockTCPClientSocket() override;

  // DeterministicSocketData::Delegate:
  bool WritePending() const override;
  bool ReadPending() const override;
  void CompleteWrite() override;
  int CompleteRead() override;

  // Socket:
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;

  // StreamSocket:
  int Connect(const CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  bool WasNpnNegotiated() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;

 private:
  DeterministicSocketHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(DeterministicMockTCPClientSocket);
};

class MockSSLClientSocket : public MockClientSocket, public AsyncSocket {
 public:
  MockSSLClientSocket(scoped_ptr<ClientSocketHandle> transport_socket,
                      const HostPortPair& host_and_port,
                      const SSLConfig& ssl_config,
                      SSLSocketDataProvider* socket);
  ~MockSSLClientSocket() override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;

  // StreamSocket implementation.
  int Connect(const CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;

  // SSLClientSocket implementation.
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override;
  NextProtoStatus GetNextProto(std::string* proto) const override;

  // This MockSocket does not implement the manual async IO feature.
  void OnReadComplete(const MockRead& data) override;
  void OnWriteComplete(int rv) override;
  void OnConnectComplete(const MockConnect& data) override;

  ChannelIDService* GetChannelIDService() const override;

 private:
  static void ConnectCallback(MockSSLClientSocket* ssl_client_socket,
                              const CompletionCallback& callback,
                              int rv);

  scoped_ptr<ClientSocketHandle> transport_;
  SSLSocketDataProvider* data_;

  DISALLOW_COPY_AND_ASSIGN(MockSSLClientSocket);
};

class MockUDPClientSocket : public DatagramClientSocket, public AsyncSocket {
 public:
  MockUDPClientSocket(SocketDataProvider* data, net::NetLog* net_log);
  ~MockUDPClientSocket() override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32 size) override;
  int SetSendBufferSize(int32 size) override;

  // DatagramSocket implementation.
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const BoundNetLog& NetLog() const override;

  // DatagramClientSocket implementation.
  int BindToNetwork(NetworkChangeNotifier::NetworkHandle network) override;
  int Connect(const IPEndPoint& address) override;

  // AsyncSocket implementation.
  void OnReadComplete(const MockRead& data) override;
  void OnWriteComplete(int rv) override;
  void OnConnectComplete(const MockConnect& data) override;

  void set_source_port(uint16 port) { source_port_ = port;}

 private:
  int CompleteRead();

  void RunCallbackAsync(const CompletionCallback& callback, int result);
  void RunCallback(const CompletionCallback& callback, int result);

  bool connected_;
  SocketDataProvider* data_;
  int read_offset_;
  MockRead read_data_;
  bool need_read_data_;
  uint16 source_port_;  // Ephemeral source port.

  // Address of the "remote" peer we're connected to.
  IPEndPoint peer_addr_;

  // While an asynchronous IO is pending, we save our user-buffer state.
  scoped_refptr<IOBuffer> pending_read_buf_;
  int pending_read_buf_len_;
  CompletionCallback pending_read_callback_;
  CompletionCallback pending_write_callback_;

  BoundNetLog net_log_;

  base::WeakPtrFactory<MockUDPClientSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockUDPClientSocket);
};

class TestSocketRequest : public TestCompletionCallbackBase {
 public:
  TestSocketRequest(std::vector<TestSocketRequest*>* request_order,
                    size_t* completion_count);
  ~TestSocketRequest() override;

  ClientSocketHandle* handle() { return &handle_; }

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result);

  ClientSocketHandle handle_;
  std::vector<TestSocketRequest*>* request_order_;
  size_t* completion_count_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketRequest);
};

class ClientSocketPoolTest {
 public:
  enum KeepAlive {
    KEEP_ALIVE,

    // A socket will be disconnected in addition to handle being reset.
    NO_KEEP_ALIVE,
  };

  static const int kIndexOutOfBounds;
  static const int kRequestNotFound;

  ClientSocketPoolTest();
  ~ClientSocketPoolTest();

  template <typename PoolType>
  int StartRequestUsingPool(
      PoolType* socket_pool,
      const std::string& group_name,
      RequestPriority priority,
      const scoped_refptr<typename PoolType::SocketParams>& socket_params) {
    DCHECK(socket_pool);
    TestSocketRequest* request =
        new TestSocketRequest(&request_order_, &completion_count_);
    requests_.push_back(request);
    int rv = request->handle()->Init(group_name,
                                     socket_params,
                                     priority,
                                     request->callback(),
                                     socket_pool,
                                     BoundNetLog());
    if (rv != ERR_IO_PENDING)
      request_order_.push_back(request);
    return rv;
  }

  // Provided there were n requests started, takes |index| in range 1..n
  // and returns order in which that request completed, in range 1..n,
  // or kIndexOutOfBounds if |index| is out of bounds, or kRequestNotFound
  // if that request did not complete (for example was canceled).
  int GetOrderOfRequest(size_t index) const;

  // Resets first initialized socket handle from |requests_|. If found such
  // a handle, returns true.
  bool ReleaseOneConnection(KeepAlive keep_alive);

  // Releases connections until there is nothing to release.
  void ReleaseAllConnections(KeepAlive keep_alive);

  // Note that this uses 0-based indices, while GetOrderOfRequest takes and
  // returns 0-based indices.
  TestSocketRequest* request(int i) { return requests_[i]; }

  size_t requests_size() const { return requests_.size(); }
  ScopedVector<TestSocketRequest>* requests() { return &requests_; }
  size_t completion_count() const { return completion_count_; }

 private:
  ScopedVector<TestSocketRequest> requests_;
  std::vector<TestSocketRequest*> request_order_;
  size_t completion_count_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolTest);
};

class MockTransportSocketParams
    : public base::RefCounted<MockTransportSocketParams> {
 private:
  friend class base::RefCounted<MockTransportSocketParams>;
  ~MockTransportSocketParams() {}

  DISALLOW_COPY_AND_ASSIGN(MockTransportSocketParams);
};

class MockTransportClientSocketPool : public TransportClientSocketPool {
 public:
  typedef MockTransportSocketParams SocketParams;

  class MockConnectJob {
   public:
    MockConnectJob(scoped_ptr<StreamSocket> socket,
                   ClientSocketHandle* handle,
                   const CompletionCallback& callback);
    ~MockConnectJob();

    int Connect();
    bool CancelHandle(const ClientSocketHandle* handle);

   private:
    void OnConnect(int rv);

    scoped_ptr<StreamSocket> socket_;
    ClientSocketHandle* handle_;
    CompletionCallback user_callback_;

    DISALLOW_COPY_AND_ASSIGN(MockConnectJob);
  };

  MockTransportClientSocketPool(int max_sockets,
                                int max_sockets_per_group,
                                ClientSocketFactory* socket_factory);

  ~MockTransportClientSocketPool() override;

  RequestPriority last_request_priority() const {
    return last_request_priority_;
  }
  int release_count() const { return release_count_; }
  int cancel_count() const { return cancel_count_; }

  // TransportClientSocketPool implementation.
  int RequestSocket(const std::string& group_name,
                    const void* socket_params,
                    RequestPriority priority,
                    ClientSocketHandle* handle,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log) override;

  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) override;
  void ReleaseSocket(const std::string& group_name,
                     scoped_ptr<StreamSocket> socket,
                     int id) override;

 private:
  ClientSocketFactory* client_socket_factory_;
  ScopedVector<MockConnectJob> job_list_;
  RequestPriority last_request_priority_;
  int release_count_;
  int cancel_count_;

  DISALLOW_COPY_AND_ASSIGN(MockTransportClientSocketPool);
};

class DeterministicMockClientSocketFactory : public ClientSocketFactory {
 public:
  DeterministicMockClientSocketFactory();
  ~DeterministicMockClientSocketFactory() override;

  void AddSocketDataProvider(DeterministicSocketData* socket);
  void AddSSLSocketDataProvider(SSLSocketDataProvider* socket);
  void ResetNextMockIndexes();

  // Return |index|-th MockSSLClientSocket (starting from 0) that the factory
  // created.
  MockSSLClientSocket* GetMockSSLClientSocket(size_t index) const;

  SocketDataProviderArray<DeterministicSocketData>& mock_data() {
    return mock_data_;
  }
  std::vector<DeterministicMockTCPClientSocket*>& tcp_client_sockets() {
    return tcp_client_sockets_;
  }
  std::vector<DeterministicMockUDPClientSocket*>& udp_client_sockets() {
    return udp_client_sockets_;
  }

  // ClientSocketFactory
  scoped_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) override;
  scoped_ptr<StreamSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* net_log,
      const NetLog::Source& source) override;
  scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<ClientSocketHandle> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      const SSLClientSocketContext& context) override;
  void ClearSSLSessionCache() override;

 private:
  SocketDataProviderArray<DeterministicSocketData> mock_data_;
  SocketDataProviderArray<SSLSocketDataProvider> mock_ssl_data_;

  // Store pointers to handed out sockets in case the test wants to get them.
  std::vector<DeterministicMockTCPClientSocket*> tcp_client_sockets_;
  std::vector<DeterministicMockUDPClientSocket*> udp_client_sockets_;
  std::vector<MockSSLClientSocket*> ssl_client_sockets_;

  DISALLOW_COPY_AND_ASSIGN(DeterministicMockClientSocketFactory);
};

class MockSOCKSClientSocketPool : public SOCKSClientSocketPool {
 public:
  MockSOCKSClientSocketPool(int max_sockets,
                            int max_sockets_per_group,
                            TransportClientSocketPool* transport_pool);

  ~MockSOCKSClientSocketPool() override;

  // SOCKSClientSocketPool implementation.
  int RequestSocket(const std::string& group_name,
                    const void* socket_params,
                    RequestPriority priority,
                    ClientSocketHandle* handle,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log) override;

  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) override;
  void ReleaseSocket(const std::string& group_name,
                     scoped_ptr<StreamSocket> socket,
                     int id) override;

 private:
  TransportClientSocketPool* const transport_pool_;

  DISALLOW_COPY_AND_ASSIGN(MockSOCKSClientSocketPool);
};

// Convenience class to temporarily set the WebSocketEndpointLockManager unlock
// delay to zero for testing purposes. Automatically restores the original value
// when destroyed.
class ScopedWebSocketEndpointZeroUnlockDelay {
 public:
  ScopedWebSocketEndpointZeroUnlockDelay();
  ~ScopedWebSocketEndpointZeroUnlockDelay();

 private:
  base::TimeDelta old_delay_;
};

// Constants for a successful SOCKS v5 handshake.
extern const char kSOCKS5GreetRequest[];
extern const int kSOCKS5GreetRequestLength;

extern const char kSOCKS5GreetResponse[];
extern const int kSOCKS5GreetResponseLength;

extern const char kSOCKS5OkRequest[];
extern const int kSOCKS5OkRequestLength;

extern const char kSOCKS5OkResponse[];
extern const int kSOCKS5OkResponseLength;

// Helper function to get the total data size of the MockReads in |reads|.
int64_t CountReadBytes(const MockRead reads[], size_t reads_size);

// Helper function to get the total data size of the MockWrites in |writes|.
int64_t CountWriteBytes(const MockWrite writes[], size_t writes_size);

}  // namespace net

#endif  // NET_SOCKET_SOCKET_TEST_UTIL_H_
