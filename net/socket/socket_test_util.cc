// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_test_util.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/auth.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket/socket.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_failure_state.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#define NET_TRACE(level, s) VLOG(level) << s << __FUNCTION__ << "() "

namespace net {

namespace {

inline char AsciifyHigh(char x) {
  char nybble = static_cast<char>((x >> 4) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char AsciifyLow(char x) {
  char nybble = static_cast<char>((x >> 0) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char Asciify(char x) {
  if ((x < 0) || !isprint(x))
    return '.';
  return x;
}

void DumpData(const char* data, int data_len) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DVLOG(1) << "Length:  " << data_len;
  const char* pfx = "Data:    ";
  if (!data || (data_len <= 0)) {
    DVLOG(1) << pfx << "<None>";
  } else {
    int i;
    for (i = 0; i <= (data_len - 4); i += 4) {
      DVLOG(1) << pfx
               << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
               << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
               << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
               << AsciifyHigh(data[i + 3]) << AsciifyLow(data[i + 3])
               << "  '"
               << Asciify(data[i + 0])
               << Asciify(data[i + 1])
               << Asciify(data[i + 2])
               << Asciify(data[i + 3])
               << "'";
      pfx = "         ";
    }
    // Take care of any 'trailing' bytes, if data_len was not a multiple of 4.
    switch (data_len - i) {
      case 3:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                 << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
                 << "    '"
                 << Asciify(data[i + 0])
                 << Asciify(data[i + 1])
                 << Asciify(data[i + 2])
                 << " '";
        break;
      case 2:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                 << "      '"
                 << Asciify(data[i + 0])
                 << Asciify(data[i + 1])
                 << "  '";
        break;
      case 1:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << "        '"
                 << Asciify(data[i + 0])
                 << "   '";
        break;
    }
  }
}

template <MockReadWriteType type>
void DumpMockReadWrite(const MockReadWrite<type>& r) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DVLOG(1) << "Async:   " << (r.mode == ASYNC)
           << "\nResult:  " << r.result;
  DumpData(r.data, r.data_len);
  const char* stop = (r.sequence_number & MockRead::STOPLOOP) ? " (STOP)" : "";
  DVLOG(1) << "Stage:   " << (r.sequence_number & ~MockRead::STOPLOOP) << stop;
}

}  // namespace

MockConnect::MockConnect() : mode(ASYNC), result(OK) {
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  peer_addr = IPEndPoint(ip, 0);
}

MockConnect::MockConnect(IoMode io_mode, int r) : mode(io_mode), result(r) {
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  peer_addr = IPEndPoint(ip, 0);
}

MockConnect::MockConnect(IoMode io_mode, int r, IPEndPoint addr) :
    mode(io_mode),
    result(r),
    peer_addr(addr) {
}

MockConnect::~MockConnect() {}

StaticSocketDataHelper::StaticSocketDataHelper(MockRead* reads,
                                               size_t reads_count,
                                               MockWrite* writes,
                                               size_t writes_count)
    : reads_(reads),
      read_index_(0),
      read_count_(reads_count),
      writes_(writes),
      write_index_(0),
      write_count_(writes_count) {
}

StaticSocketDataHelper::~StaticSocketDataHelper() {
}

const MockRead& StaticSocketDataHelper::PeekRead() const {
  CHECK(!AllReadDataConsumed());
  return reads_[read_index_];
}

const MockWrite& StaticSocketDataHelper::PeekWrite() const {
  CHECK(!AllWriteDataConsumed());
  return writes_[write_index_];
}

const MockRead& StaticSocketDataHelper::AdvanceRead() {
  CHECK(!AllReadDataConsumed());
  return reads_[read_index_++];
}

const MockWrite& StaticSocketDataHelper::AdvanceWrite() {
  CHECK(!AllWriteDataConsumed());
  return writes_[write_index_++];
}

bool StaticSocketDataHelper::VerifyWriteData(const std::string& data) {
  CHECK(!AllWriteDataConsumed());
  // Check that what the actual data matches the expectations.
  const MockWrite& next_write = PeekWrite();
  if (!next_write.data)
    return true;

  // Note: Partial writes are supported here.  If the expected data
  // is a match, but shorter than the write actually written, that is legal.
  // Example:
  //   Application writes "foobarbaz" (9 bytes)
  //   Expected write was "foo" (3 bytes)
  //   This is a success, and the function returns true.
  std::string expected_data(next_write.data, next_write.data_len);
  std::string actual_data(data.substr(0, next_write.data_len));
  EXPECT_GE(data.length(), expected_data.length());
  EXPECT_EQ(expected_data, actual_data);
  return expected_data == actual_data;
}

void StaticSocketDataHelper::Reset() {
  read_index_ = 0;
  write_index_ = 0;
}

StaticSocketDataProvider::StaticSocketDataProvider()
    : StaticSocketDataProvider(nullptr, 0, nullptr, 0) {
}

StaticSocketDataProvider::StaticSocketDataProvider(MockRead* reads,
                                                   size_t reads_count,
                                                   MockWrite* writes,
                                                   size_t writes_count)
    : helper_(reads, reads_count, writes, writes_count) {
}

StaticSocketDataProvider::~StaticSocketDataProvider() {
}

MockRead StaticSocketDataProvider::OnRead() {
  CHECK(!helper_.AllReadDataConsumed());
  return helper_.AdvanceRead();
}

MockWriteResult StaticSocketDataProvider::OnWrite(const std::string& data) {
  if (helper_.write_count() == 0) {
    // Not using mock writes; succeed synchronously.
    return MockWriteResult(SYNCHRONOUS, data.length());
  }
  EXPECT_FALSE(helper_.AllWriteDataConsumed());
  if (helper_.AllWriteDataConsumed()) {
    // Show what the extra write actually consists of.
    EXPECT_EQ("<unexpected write>", data);
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
  }

  // Check that what we are writing matches the expectation.
  // Then give the mocked return value.
  if (!helper_.VerifyWriteData(data))
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);

  const MockWrite& next_write = helper_.AdvanceWrite();
  // In the case that the write was successful, return the number of bytes
  // written. Otherwise return the error code.
  int result =
      next_write.result == OK ? next_write.data_len : next_write.result;
  return MockWriteResult(next_write.mode, result);
}

void StaticSocketDataProvider::Reset() {
  helper_.Reset();
}

bool StaticSocketDataProvider::AllReadDataConsumed() const {
  return helper_.AllReadDataConsumed();
}

bool StaticSocketDataProvider::AllWriteDataConsumed() const {
  return helper_.AllWriteDataConsumed();
}

SSLSocketDataProvider::SSLSocketDataProvider(IoMode mode, int result)
    : connect(mode, result),
      next_proto_status(SSLClientSocket::kNextProtoUnsupported),
      client_cert_sent(false),
      cert_request_info(NULL),
      channel_id_sent(false),
      connection_status(0) {
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2,
                                &connection_status);
  // Set to TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305
  SSLConnectionStatusSetCipherSuite(0xcc14, &connection_status);
}

SSLSocketDataProvider::~SSLSocketDataProvider() {
}

void SSLSocketDataProvider::SetNextProto(NextProto proto) {
  next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  next_proto = SSLClientSocket::NextProtoToString(proto);
}

SequencedSocketData::SequencedSocketData(MockRead* reads,
                                         size_t reads_count,
                                         MockWrite* writes,
                                         size_t writes_count)
    : helper_(reads, reads_count, writes, writes_count),
      sequence_number_(0),
      read_state_(IDLE),
      write_state_(IDLE),
      weak_factory_(this) {
  // Check that reads and writes have a contiguous set of sequence numbers
  // starting from 0 and working their way up, with no repeats and skipping
  // no values.
  size_t next_read = 0;
  size_t next_write = 0;
  int next_sequence_number = 0;
  while (next_read < reads_count || next_write < writes_count) {
    if (next_read < reads_count &&
        reads[next_read].sequence_number == next_sequence_number) {
      ++next_read;
      ++next_sequence_number;
      continue;
    }
    if (next_write < writes_count &&
        writes[next_write].sequence_number == next_sequence_number) {
      ++next_write;
      ++next_sequence_number;
      continue;
    }
    CHECK(false) << "Sequence number not found where expected: "
                 << next_sequence_number;
    return;
  }
  CHECK_EQ(next_read, reads_count);
  CHECK_EQ(next_write, writes_count);
}

SequencedSocketData::SequencedSocketData(const MockConnect& connect,
                                         MockRead* reads,
                                         size_t reads_count,
                                         MockWrite* writes,
                                         size_t writes_count)
    : SequencedSocketData(reads, reads_count, writes, writes_count) {
  set_connect_data(connect);
}

MockRead SequencedSocketData::OnRead() {
  CHECK_EQ(IDLE, read_state_);
  CHECK(!helper_.AllReadDataConsumed());

  NET_TRACE(1, " *** ") << "sequence_number: " << sequence_number_;
  const MockRead& next_read = helper_.PeekRead();
  NET_TRACE(1, " *** ") << "next_read: " << next_read.sequence_number;
  CHECK_GE(next_read.sequence_number, sequence_number_);

  if (next_read.sequence_number <= sequence_number_) {
    if (next_read.mode == SYNCHRONOUS) {
      NET_TRACE(1, " *** ") << "Returning synchronously";
      DumpMockReadWrite(next_read);
      helper_.AdvanceRead();
      ++sequence_number_;
      MaybePostWriteCompleteTask();
      return next_read;
    }

    // If the result is ERR_IO_PENDING, then advance to the next state
    // and pause reads.
    if (next_read.result == ERR_IO_PENDING) {
      NET_TRACE(1, " *** ") << "Pausing at: " << sequence_number_;
      ++sequence_number_;
      helper_.AdvanceRead();
      read_state_ = PAUSED;
      return MockRead(SYNCHRONOUS, ERR_IO_PENDING);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SequencedSocketData::OnReadComplete,
                              weak_factory_.GetWeakPtr()));
    CHECK_NE(COMPLETING, write_state_);
    read_state_ = COMPLETING;
  } else if (next_read.mode == SYNCHRONOUS) {
    ADD_FAILURE() << "Unable to perform synchronous IO while stopped";
    return MockRead(SYNCHRONOUS, ERR_UNEXPECTED);
  } else {
    NET_TRACE(1, " *** ") << "Waiting for write to trigger read";
    read_state_ = PENDING;
  }

  return MockRead(SYNCHRONOUS, ERR_IO_PENDING);
}

MockWriteResult SequencedSocketData::OnWrite(const std::string& data) {
  CHECK_EQ(IDLE, write_state_);
  CHECK(!helper_.AllWriteDataConsumed());

  NET_TRACE(1, " *** ") << "sequence_number: " << sequence_number_;
  const MockWrite& next_write = helper_.PeekWrite();
  NET_TRACE(1, " *** ") << "next_write: " << next_write.sequence_number;
  CHECK_GE(next_write.sequence_number, sequence_number_);

  if (!helper_.VerifyWriteData(data))
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);

  if (next_write.sequence_number <= sequence_number_) {
    if (next_write.mode == SYNCHRONOUS) {
      helper_.AdvanceWrite();
      ++sequence_number_;
      MaybePostReadCompleteTask();
      // In the case that the write was successful, return the number of bytes
      // written. Otherwise return the error code.
      int rv =
          next_write.result != OK ? next_write.result : next_write.data_len;
      NET_TRACE(1, " *** ") << "Returning synchronously";
      return MockWriteResult(SYNCHRONOUS, rv);
    }

    NET_TRACE(1, " *** ") << "Posting task to complete write";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SequencedSocketData::OnWriteComplete,
                              weak_factory_.GetWeakPtr()));
    CHECK_NE(COMPLETING, read_state_);
    write_state_ = COMPLETING;
  } else if (next_write.mode == SYNCHRONOUS) {
    ADD_FAILURE() << "Unable to perform synchronous IO while stopped";
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
  } else {
    NET_TRACE(1, " *** ") << "Waiting for read to trigger write";
    write_state_ = PENDING;
  }

  return MockWriteResult(SYNCHRONOUS, ERR_IO_PENDING);
}

void SequencedSocketData::Reset() {
  helper_.Reset();
  sequence_number_ = 0;
  read_state_ = IDLE;
  write_state_ = IDLE;
  weak_factory_.InvalidateWeakPtrs();
}

bool SequencedSocketData::AllReadDataConsumed() const {
  return helper_.AllReadDataConsumed();
}

bool SequencedSocketData::AllWriteDataConsumed() const {
  return helper_.AllWriteDataConsumed();
}

bool SequencedSocketData::IsReadPaused() {
  return read_state_ == PAUSED;
}

void SequencedSocketData::CompleteRead() {
  if (read_state_ != PAUSED) {
    ADD_FAILURE() << "Unable to CompleteRead when not paused.";
    return;
  }
  read_state_ = COMPLETING;
  OnReadComplete();
}

void SequencedSocketData::MaybePostReadCompleteTask() {
  NET_TRACE(1, " ****** ") << " current: " << sequence_number_;
  // Only trigger the next read to complete if there is already a read pending
  // which should complete at the current sequence number.
  if (read_state_ != PENDING ||
      helper_.PeekRead().sequence_number != sequence_number_) {
    return;
  }

  // If the result is ERR_IO_PENDING, then advance to the next state
  // and pause reads.
  if (helper_.PeekRead().result == ERR_IO_PENDING) {
    NET_TRACE(1, " *** ") << "Pausing read at: " << sequence_number_;
    ++sequence_number_;
    helper_.AdvanceRead();
    read_state_ = PAUSED;
    return;
  }

  NET_TRACE(1, " ****** ") << "Posting task to complete read: "
                           << sequence_number_;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&SequencedSocketData::OnReadComplete,
                            weak_factory_.GetWeakPtr()));
  CHECK_NE(COMPLETING, write_state_);
  read_state_ = COMPLETING;
}

void SequencedSocketData::MaybePostWriteCompleteTask() {
  NET_TRACE(1, " ****** ") << " current: " << sequence_number_;
  // Only trigger the next write to complete if there is already a write pending
  // which should complete at the current sequence number.
  if (write_state_ != PENDING ||
      helper_.PeekWrite().sequence_number != sequence_number_) {
    return;
  }

  NET_TRACE(1, " ****** ") << "Posting task to complete write: "
                           << sequence_number_;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&SequencedSocketData::OnWriteComplete,
                            weak_factory_.GetWeakPtr()));
  CHECK_NE(COMPLETING, read_state_);
  write_state_ = COMPLETING;
}

void SequencedSocketData::OnReadComplete() {
  CHECK_EQ(COMPLETING, read_state_);
  NET_TRACE(1, " *** ") << "Completing read for: " << sequence_number_;

  MockRead data = helper_.AdvanceRead();
  DCHECK_EQ(sequence_number_, data.sequence_number);
  sequence_number_++;
  read_state_ = IDLE;

  // The result of this read completing might trigger the completion
  // of a pending write. If so, post a task to complete the write later.
  // Since the socket may call back into the SequencedSocketData
  // from socket()->OnReadComplete(), trigger the write task to be posted
  // before calling that.
  MaybePostWriteCompleteTask();

  if (!socket()) {
    NET_TRACE(1, " *** ") << "No socket available to complete read";
    return;
  }

  NET_TRACE(1, " *** ") << "Completing socket read for: "
                        << data.sequence_number;
  DumpMockReadWrite(data);
  socket()->OnReadComplete(data);
  NET_TRACE(1, " *** ") << "Done";
}

void SequencedSocketData::OnWriteComplete() {
  CHECK_EQ(COMPLETING, write_state_);
  NET_TRACE(1, " *** ") << " Completing write for: " << sequence_number_;

  const MockWrite& data = helper_.AdvanceWrite();
  DCHECK_EQ(sequence_number_, data.sequence_number);
  sequence_number_++;
  write_state_ = IDLE;
  int rv = data.result == OK ? data.data_len : data.result;

  // The result of this write completing might trigger the completion
  // of a pending read. If so, post a task to complete the read later.
  // Since the socket may call back into the SequencedSocketData
  // from socket()->OnWriteComplete(), trigger the write task to be posted
  // before calling that.
  MaybePostReadCompleteTask();

  if (!socket()) {
    NET_TRACE(1, " *** ") << "No socket available to complete write";
    return;
  }

  NET_TRACE(1, " *** ") << " Completing socket write for: "
                        << data.sequence_number;
  socket()->OnWriteComplete(rv);
  NET_TRACE(1, " *** ") << "Done";
}

SequencedSocketData::~SequencedSocketData() {
}

DeterministicSocketData::DeterministicSocketData(MockRead* reads,
                                                 size_t reads_count,
                                                 MockWrite* writes,
                                                 size_t writes_count)
    : helper_(reads, reads_count, writes, writes_count),
      sequence_number_(0),
      current_read_(),
      current_write_(),
      stopping_sequence_number_(0),
      stopped_(false),
      print_debug_(false),
      is_running_(false) {
  VerifyCorrectSequenceNumbers(reads, reads_count, writes, writes_count);
}

DeterministicSocketData::~DeterministicSocketData() {}

void DeterministicSocketData::Run() {
  DCHECK(!is_running_);
  is_running_ = true;

  SetStopped(false);
  int counter = 0;
  // Continue to consume data until all data has run out, or the stopped_ flag
  // has been set. Consuming data requires two separate operations -- running
  // the tasks in the message loop, and explicitly invoking the read/write
  // callbacks (simulating network I/O). We check our conditions between each,
  // since they can change in either.
  while ((!AllWriteDataConsumed() || !AllReadDataConsumed()) && !stopped()) {
    if (counter % 2 == 0)
      base::RunLoop().RunUntilIdle();
    if (counter % 2 == 1) {
      InvokeCallbacks();
    }
    counter++;
  }
  // We're done consuming new data, but it is possible there are still some
  // pending callbacks which we expect to complete before returning.
  while (delegate_.get() &&
         (delegate_->WritePending() || delegate_->ReadPending()) &&
         !stopped()) {
    InvokeCallbacks();
    base::RunLoop().RunUntilIdle();
  }
  SetStopped(false);
  is_running_ = false;
}

void DeterministicSocketData::RunFor(int steps) {
  StopAfter(steps);
  Run();
}

void DeterministicSocketData::SetStop(int seq) {
  DCHECK_LT(sequence_number_, seq);
  stopping_sequence_number_ = seq;
  stopped_ = false;
}

void DeterministicSocketData::StopAfter(int seq) {
  SetStop(sequence_number_ + seq);
}

MockRead DeterministicSocketData::OnRead() {
  current_read_ = helper_.PeekRead();

  // Synchronous read while stopped is an error
  if (stopped() && current_read_.mode == SYNCHRONOUS) {
    LOG(ERROR) << "Unable to perform synchronous IO while stopped";
    return MockRead(SYNCHRONOUS, ERR_UNEXPECTED);
  }

  // Async read which will be called back in a future step.
  if (sequence_number_ < current_read_.sequence_number) {
    NET_TRACE(1, "  *** ") << "Stage " << sequence_number_ << ": I/O Pending";
    MockRead result = MockRead(SYNCHRONOUS, ERR_IO_PENDING);
    if (current_read_.mode == SYNCHRONOUS) {
      LOG(ERROR) << "Unable to perform synchronous read: "
          << current_read_.sequence_number
          << " at stage: " << sequence_number_;
      result = MockRead(SYNCHRONOUS, ERR_UNEXPECTED);
    }
    if (print_debug_)
      DumpMockReadWrite(result);
    return result;
  }

  NET_TRACE(1, "  *** ") << "Stage " << sequence_number_ << ": Read "
                         << helper_.read_index();
  if (print_debug_)
    DumpMockReadWrite(current_read_);

  // Increment the sequence number if IO is complete
  if (current_read_.mode == SYNCHRONOUS)
    NextStep();

  DCHECK_NE(ERR_IO_PENDING, current_read_.result);

  helper_.AdvanceRead();
  return current_read_;
}

MockWriteResult DeterministicSocketData::OnWrite(const std::string& data) {
  const MockWrite& next_write = helper_.PeekWrite();
  current_write_ = next_write;

  // Synchronous write while stopped is an error
  if (stopped() && next_write.mode == SYNCHRONOUS) {
    LOG(ERROR) << "Unable to perform synchronous IO while stopped";
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
  }

  // Async write which will be called back in a future step.
  if (sequence_number_ < next_write.sequence_number) {
    NET_TRACE(1, "  *** ") << "Stage " << sequence_number_ << ": I/O Pending";
    if (next_write.mode == SYNCHRONOUS) {
      LOG(ERROR) << "Unable to perform synchronous write: "
          << next_write.sequence_number << " at stage: " << sequence_number_;
      return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);
    }
  } else {
    NET_TRACE(1, "  *** ") << "Stage " << sequence_number_ << ": Write "
                           << helper_.write_index();
  }

  if (print_debug_)
    DumpMockReadWrite(next_write);

  // Move to the next step if I/O is synchronous, since the operation will
  // complete when this method returns.
  if (next_write.mode == SYNCHRONOUS)
    NextStep();

  // Check that what we are writing matches the expectation.
  // Then give the mocked return value.
  if (!helper_.VerifyWriteData(data))
    return MockWriteResult(SYNCHRONOUS, ERR_UNEXPECTED);

  helper_.AdvanceWrite();

  // In the case that the write was successful, return the number of bytes
  // written. Otherwise return the error code.
  int result =
      next_write.result == OK ? next_write.data_len : next_write.result;
  return MockWriteResult(next_write.mode, result);
}

bool DeterministicSocketData::AllReadDataConsumed() const {
  return helper_.AllReadDataConsumed();
}

bool DeterministicSocketData::AllWriteDataConsumed() const {
  return helper_.AllWriteDataConsumed();
}

void DeterministicSocketData::InvokeCallbacks() {
  if (delegate_.get() && delegate_->WritePending() &&
      (current_write().sequence_number == sequence_number())) {
    NextStep();
    delegate_->CompleteWrite();
    return;
  }
  if (delegate_.get() && delegate_->ReadPending() &&
      (current_read().sequence_number == sequence_number())) {
    NextStep();
    delegate_->CompleteRead();
    return;
  }
}

void DeterministicSocketData::NextStep() {
  // Invariant: Can never move *past* the stopping step.
  DCHECK_LT(sequence_number_, stopping_sequence_number_);
  sequence_number_++;
  if (sequence_number_ == stopping_sequence_number_)
    SetStopped(true);
}

void DeterministicSocketData::VerifyCorrectSequenceNumbers(
    MockRead* reads, size_t reads_count,
    MockWrite* writes, size_t writes_count) {
  size_t read = 0;
  size_t write = 0;
  int expected = 0;
  while (read < reads_count || write < writes_count) {
    // Check to see that we have a read or write at the expected
    // state.
    if (read < reads_count  && reads[read].sequence_number == expected) {
      ++read;
      ++expected;
      continue;
    }
    if (write < writes_count && writes[write].sequence_number == expected) {
      ++write;
      ++expected;
      continue;
    }
    NOTREACHED() << "Missing sequence number: " << expected;
    return;
  }
  DCHECK_EQ(read, reads_count);
  DCHECK_EQ(write, writes_count);
}

MockClientSocketFactory::MockClientSocketFactory() {}

MockClientSocketFactory::~MockClientSocketFactory() {}

void MockClientSocketFactory::AddSocketDataProvider(
    SocketDataProvider* data) {
  mock_data_.Add(data);
}

void MockClientSocketFactory::AddSSLSocketDataProvider(
    SSLSocketDataProvider* data) {
  mock_ssl_data_.Add(data);
}

void MockClientSocketFactory::ResetNextMockIndexes() {
  mock_data_.ResetNextIndex();
  mock_ssl_data_.ResetNextIndex();
}

scoped_ptr<DatagramClientSocket>
MockClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    const RandIntCallback& rand_int_cb,
    NetLog* net_log,
    const NetLog::Source& source) {
  SocketDataProvider* data_provider = mock_data_.GetNext();
  scoped_ptr<MockUDPClientSocket> socket(
      new MockUDPClientSocket(data_provider, net_log));
  data_provider->set_socket(socket.get());
  if (bind_type == DatagramSocket::RANDOM_BIND)
    socket->set_source_port(static_cast<uint16>(rand_int_cb.Run(1025, 65535)));
  return socket.Pass();
}

scoped_ptr<StreamSocket> MockClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    NetLog* net_log,
    const NetLog::Source& source) {
  SocketDataProvider* data_provider = mock_data_.GetNext();
  scoped_ptr<MockTCPClientSocket> socket(
      new MockTCPClientSocket(addresses, net_log, data_provider));
  data_provider->set_socket(socket.get());
  return socket.Pass();
}

scoped_ptr<SSLClientSocket> MockClientSocketFactory::CreateSSLClientSocket(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context) {
  SSLSocketDataProvider* next_ssl_data = mock_ssl_data_.GetNext();
  if (!next_ssl_data->next_protos_expected_in_ssl_config.empty()) {
    EXPECT_EQ(next_ssl_data->next_protos_expected_in_ssl_config.size(),
              ssl_config.next_protos.size());
    EXPECT_TRUE(
        std::equal(next_ssl_data->next_protos_expected_in_ssl_config.begin(),
                   next_ssl_data->next_protos_expected_in_ssl_config.end(),
                   ssl_config.next_protos.begin()));
  }
  return scoped_ptr<SSLClientSocket>(new MockSSLClientSocket(
      transport_socket.Pass(), host_and_port, ssl_config, next_ssl_data));
}

void MockClientSocketFactory::ClearSSLSessionCache() {
}

const char MockClientSocket::kTlsUnique[] = "MOCK_TLSUNIQ";

MockClientSocket::MockClientSocket(const BoundNetLog& net_log)
    : connected_(false),
      net_log_(net_log),
      weak_factory_(this) {
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  peer_addr_ = IPEndPoint(ip, 0);
}

int MockClientSocket::SetReceiveBufferSize(int32 size) {
  return OK;
}

int MockClientSocket::SetSendBufferSize(int32 size) {
  return OK;
}

void MockClientSocket::Disconnect() {
  connected_ = false;
}

bool MockClientSocket::IsConnected() const {
  return connected_;
}

bool MockClientSocket::IsConnectedAndIdle() const {
  return connected_;
}

int MockClientSocket::GetPeerAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = peer_addr_;
  return OK;
}

int MockClientSocket::GetLocalAddress(IPEndPoint* address) const {
  IPAddressNumber ip;
  bool rv = ParseIPLiteralToNumber("192.0.2.33", &ip);
  CHECK(rv);
  *address = IPEndPoint(ip, 123);
  return OK;
}

const BoundNetLog& MockClientSocket::NetLog() const {
  return net_log_;
}

void MockClientSocket::GetConnectionAttempts(ConnectionAttempts* out) const {
  out->clear();
}

int64_t MockClientSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void MockClientSocket::GetSSLCertRequestInfo(
  SSLCertRequestInfo* cert_request_info) {
}

int MockClientSocket::ExportKeyingMaterial(const base::StringPiece& label,
                                           bool has_context,
                                           const base::StringPiece& context,
                                           unsigned char* out,
                                           unsigned int outlen) {
  memset(out, 'A', outlen);
  return OK;
}

int MockClientSocket::GetTLSUniqueChannelBinding(std::string* out) {
  out->assign(MockClientSocket::kTlsUnique);
  return OK;
}

ChannelIDService* MockClientSocket::GetChannelIDService() const {
  NOTREACHED();
  return NULL;
}

SSLFailureState MockClientSocket::GetSSLFailureState() const {
  return IsConnected() ? SSL_FAILURE_NONE : SSL_FAILURE_UNKNOWN;
}

SSLClientSocket::NextProtoStatus MockClientSocket::GetNextProto(
    std::string* proto) const {
  proto->clear();
  return SSLClientSocket::kNextProtoUnsupported;
}

MockClientSocket::~MockClientSocket() {}

void MockClientSocket::RunCallbackAsync(const CompletionCallback& callback,
                                        int result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&MockClientSocket::RunCallback,
                            weak_factory_.GetWeakPtr(), callback, result));
}

void MockClientSocket::RunCallback(const CompletionCallback& callback,
                                   int result) {
  if (!callback.is_null())
    callback.Run(result);
}

MockTCPClientSocket::MockTCPClientSocket(const AddressList& addresses,
                                         net::NetLog* net_log,
                                         SocketDataProvider* data)
    : MockClientSocket(BoundNetLog::Make(net_log, NetLog::SOURCE_NONE)),
      addresses_(addresses),
      data_(data),
      read_offset_(0),
      read_data_(SYNCHRONOUS, ERR_UNEXPECTED),
      need_read_data_(true),
      peer_closed_connection_(false),
      pending_read_buf_(NULL),
      pending_read_buf_len_(0),
      was_used_to_convey_data_(false) {
  DCHECK(data_);
  peer_addr_ = data->connect_data().peer_addr;
  data_->Reset();
}

MockTCPClientSocket::~MockTCPClientSocket() {}

int MockTCPClientSocket::Read(IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  // If the buffer is already in use, a read is already in progress!
  DCHECK(pending_read_buf_.get() == NULL);

  // Store our async IO data.
  pending_read_buf_ = buf;
  pending_read_buf_len_ = buf_len;
  pending_read_callback_ = callback;

  if (need_read_data_) {
    read_data_ = data_->OnRead();
    if (read_data_.result == ERR_CONNECTION_CLOSED) {
      // This MockRead is just a marker to instruct us to set
      // peer_closed_connection_.
      peer_closed_connection_ = true;
    }
    if (read_data_.result == ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ) {
      // This MockRead is just a marker to instruct us to set
      // peer_closed_connection_.  Skip it and get the next one.
      read_data_ = data_->OnRead();
      peer_closed_connection_ = true;
    }
    // ERR_IO_PENDING means that the SocketDataProvider is taking responsibility
    // to complete the async IO manually later (via OnReadComplete).
    if (read_data_.result == ERR_IO_PENDING) {
      // We need to be using async IO in this case.
      DCHECK(!callback.is_null());
      return ERR_IO_PENDING;
    }
    need_read_data_ = false;
  }

  return CompleteRead();
}

int MockTCPClientSocket::Write(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  if (!connected_)
    return ERR_UNEXPECTED;

  std::string data(buf->data(), buf_len);
  MockWriteResult write_result = data_->OnWrite(data);

  was_used_to_convey_data_ = true;

  // ERR_IO_PENDING is a signal that the socket data will call back
  // asynchronously later.
  if (write_result.result == ERR_IO_PENDING) {
    pending_write_callback_ = callback;
    return ERR_IO_PENDING;
  }

  // TODO(rch): remove this once OrderedSocketData and DelayedSocketData
  // have been removed.
  if (write_result.mode == ASYNC) {
    RunCallbackAsync(callback, write_result.result);
    return ERR_IO_PENDING;
  }

  return write_result.result;
}

void MockTCPClientSocket::GetConnectionAttempts(ConnectionAttempts* out) const {
  *out = connection_attempts_;
}

void MockTCPClientSocket::ClearConnectionAttempts() {
  connection_attempts_.clear();
}

void MockTCPClientSocket::AddConnectionAttempts(
    const ConnectionAttempts& attempts) {
  connection_attempts_.insert(connection_attempts_.begin(), attempts.begin(),
                              attempts.end());
}

int MockTCPClientSocket::Connect(const CompletionCallback& callback) {
  if (connected_)
    return OK;
  connected_ = true;
  peer_closed_connection_ = false;

  int result = data_->connect_data().result;
  IoMode mode = data_->connect_data().mode;

  if (result != OK && result != ERR_IO_PENDING) {
    IPEndPoint address;
    if (GetPeerAddress(&address) == OK)
      connection_attempts_.push_back(ConnectionAttempt(address, result));
  }

  if (mode == SYNCHRONOUS)
    return result;

  if (result == ERR_IO_PENDING)
    pending_connect_callback_ = callback;
  else
    RunCallbackAsync(callback, result);
  return ERR_IO_PENDING;
}

void MockTCPClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
  pending_connect_callback_.Reset();
  pending_read_callback_.Reset();
}

bool MockTCPClientSocket::IsConnected() const {
  return connected_ && !peer_closed_connection_;
}

bool MockTCPClientSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

int MockTCPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  if (addresses_.empty())
    return MockClientSocket::GetPeerAddress(address);

  *address = addresses_[0];
  return OK;
}

bool MockTCPClientSocket::WasEverUsed() const {
  return was_used_to_convey_data_;
}

bool MockTCPClientSocket::UsingTCPFastOpen() const {
  return false;
}

bool MockTCPClientSocket::WasNpnNegotiated() const {
  return false;
}

bool MockTCPClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

void MockTCPClientSocket::OnReadComplete(const MockRead& data) {
  // There must be a read pending.
  DCHECK(pending_read_buf_.get());
  // You can't complete a read with another ERR_IO_PENDING status code.
  DCHECK_NE(ERR_IO_PENDING, data.result);
  // Since we've been waiting for data, need_read_data_ should be true.
  DCHECK(need_read_data_);

  read_data_ = data;
  need_read_data_ = false;

  // The caller is simulating that this IO completes right now.  Don't
  // let CompleteRead() schedule a callback.
  read_data_.mode = SYNCHRONOUS;

  CompletionCallback callback = pending_read_callback_;
  int rv = CompleteRead();
  RunCallback(callback, rv);
}

void MockTCPClientSocket::OnWriteComplete(int rv) {
  // There must be a read pending.
  DCHECK(!pending_write_callback_.is_null());
  CompletionCallback callback = pending_write_callback_;
  RunCallback(callback, rv);
}

void MockTCPClientSocket::OnConnectComplete(const MockConnect& data) {
  CompletionCallback callback = pending_connect_callback_;
  RunCallback(callback, data.result);
}

int MockTCPClientSocket::CompleteRead() {
  DCHECK(pending_read_buf_.get());
  DCHECK(pending_read_buf_len_ > 0);

  was_used_to_convey_data_ = true;

  // Save the pending async IO data and reset our |pending_| state.
  scoped_refptr<IOBuffer> buf = pending_read_buf_;
  int buf_len = pending_read_buf_len_;
  CompletionCallback callback = pending_read_callback_;
  pending_read_buf_ = NULL;
  pending_read_buf_len_ = 0;
  pending_read_callback_.Reset();

  int result = read_data_.result;
  DCHECK(result != ERR_IO_PENDING);

  if (read_data_.data) {
    if (read_data_.data_len - read_offset_ > 0) {
      result = std::min(buf_len, read_data_.data_len - read_offset_);
      memcpy(buf->data(), read_data_.data + read_offset_, result);
      read_offset_ += result;
      if (read_offset_ == read_data_.data_len) {
        need_read_data_ = true;
        read_offset_ = 0;
      }
    } else {
      result = 0;  // EOF
    }
  }

  if (read_data_.mode == ASYNC) {
    DCHECK(!callback.is_null());
    RunCallbackAsync(callback, result);
    return ERR_IO_PENDING;
  }
  return result;
}

DeterministicSocketHelper::DeterministicSocketHelper(
    NetLog* net_log,
    DeterministicSocketData* data)
    : write_pending_(false),
      write_result_(0),
      read_data_(),
      read_buf_(NULL),
      read_buf_len_(0),
      read_pending_(false),
      data_(data),
      was_used_to_convey_data_(false),
      peer_closed_connection_(false),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_NONE)) {
}

DeterministicSocketHelper::~DeterministicSocketHelper() {}

void DeterministicSocketHelper::CompleteWrite() {
  was_used_to_convey_data_ = true;
  write_pending_ = false;
  write_callback_.Run(write_result_);
}

int DeterministicSocketHelper::CompleteRead() {
  DCHECK_GT(read_buf_len_, 0);
  DCHECK_LE(read_data_.data_len, read_buf_len_);
  DCHECK(read_buf_);

  was_used_to_convey_data_ = true;

  if (read_data_.result == ERR_IO_PENDING)
    read_data_ = data_->OnRead();
  DCHECK_NE(ERR_IO_PENDING, read_data_.result);
  // If read_data_.mode is ASYNC, we do not need to wait, since this is already
  // the callback. Therefore we don't even bother to check it.
  int result = read_data_.result;

  if (read_data_.data_len > 0) {
    DCHECK(read_data_.data);
    result = std::min(read_buf_len_, read_data_.data_len);
    memcpy(read_buf_->data(), read_data_.data, result);
  }

  if (read_pending_) {
    read_pending_ = false;
    read_callback_.Run(result);
  }

  return result;
}

int DeterministicSocketHelper::Write(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  std::string data(buf->data(), buf_len);
  MockWriteResult write_result = data_->OnWrite(data);

  if (write_result.mode == ASYNC) {
    write_callback_ = callback;
    write_result_ = write_result.result;
    DCHECK(!write_callback_.is_null());
    write_pending_ = true;
    return ERR_IO_PENDING;
  }

  was_used_to_convey_data_ = true;
  write_pending_ = false;
  return write_result.result;
}

int DeterministicSocketHelper::Read(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
  read_data_ = data_->OnRead();
  // The buffer should always be big enough to contain all the MockRead data. To
  // use small buffers, split the data into multiple MockReads.
  DCHECK_LE(read_data_.data_len, buf_len);

  if (read_data_.result == ERR_CONNECTION_CLOSED) {
    // This MockRead is just a marker to instruct us to set
    // peer_closed_connection_.
    peer_closed_connection_ = true;
  }
  if (read_data_.result == ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ) {
    // This MockRead is just a marker to instruct us to set
    // peer_closed_connection_.  Skip it and get the next one.
    read_data_ = data_->OnRead();
    peer_closed_connection_ = true;
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;

  if (read_data_.mode == ASYNC || (read_data_.result == ERR_IO_PENDING)) {
    read_pending_ = true;
    DCHECK(!read_callback_.is_null());
    return ERR_IO_PENDING;
  }

  was_used_to_convey_data_ = true;
  return CompleteRead();
}

DeterministicMockUDPClientSocket::DeterministicMockUDPClientSocket(
    net::NetLog* net_log,
    DeterministicSocketData* data)
    : connected_(false),
      helper_(net_log, data),
      source_port_(123) {
}

DeterministicMockUDPClientSocket::~DeterministicMockUDPClientSocket() {}

bool DeterministicMockUDPClientSocket::WritePending() const {
  return helper_.write_pending();
}

bool DeterministicMockUDPClientSocket::ReadPending() const {
  return helper_.read_pending();
}

void DeterministicMockUDPClientSocket::CompleteWrite() {
  helper_.CompleteWrite();
}

int DeterministicMockUDPClientSocket::CompleteRead() {
  return helper_.CompleteRead();
}

int DeterministicMockUDPClientSocket::BindToNetwork(
    NetworkChangeNotifier::NetworkHandle network) {
  return ERR_NOT_IMPLEMENTED;
}

int DeterministicMockUDPClientSocket::Connect(const IPEndPoint& address) {
  if (connected_)
    return OK;
  connected_ = true;
  peer_address_ = address;
  return helper_.data()->connect_data().result;
};

int DeterministicMockUDPClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Write(buf, buf_len, callback);
}

int DeterministicMockUDPClientSocket::Read(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Read(buf, buf_len, callback);
}

int DeterministicMockUDPClientSocket::SetReceiveBufferSize(int32 size) {
  return OK;
}

int DeterministicMockUDPClientSocket::SetSendBufferSize(int32 size) {
  return OK;
}

void DeterministicMockUDPClientSocket::Close() {
  connected_ = false;
}

int DeterministicMockUDPClientSocket::GetPeerAddress(
    IPEndPoint* address) const {
  *address = peer_address_;
  return OK;
}

int DeterministicMockUDPClientSocket::GetLocalAddress(
    IPEndPoint* address) const {
  IPAddressNumber ip;
  bool rv = ParseIPLiteralToNumber("192.0.2.33", &ip);
  CHECK(rv);
  *address = IPEndPoint(ip, source_port_);
  return OK;
}

const BoundNetLog& DeterministicMockUDPClientSocket::NetLog() const {
  return helper_.net_log();
}

DeterministicMockTCPClientSocket::DeterministicMockTCPClientSocket(
    net::NetLog* net_log,
    DeterministicSocketData* data)
    : MockClientSocket(BoundNetLog::Make(net_log, NetLog::SOURCE_NONE)),
      helper_(net_log, data) {
  peer_addr_ = data->connect_data().peer_addr;
}

DeterministicMockTCPClientSocket::~DeterministicMockTCPClientSocket() {}

bool DeterministicMockTCPClientSocket::WritePending() const {
  return helper_.write_pending();
}

bool DeterministicMockTCPClientSocket::ReadPending() const {
  return helper_.read_pending();
}

void DeterministicMockTCPClientSocket::CompleteWrite() {
  helper_.CompleteWrite();
}

int DeterministicMockTCPClientSocket::CompleteRead() {
  return helper_.CompleteRead();
}

int DeterministicMockTCPClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Write(buf, buf_len, callback);
}

int DeterministicMockTCPClientSocket::Read(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  return helper_.Read(buf, buf_len, callback);
}

// TODO(erikchen): Support connect sequencing.
int DeterministicMockTCPClientSocket::Connect(
    const CompletionCallback& callback) {
  if (connected_)
    return OK;
  connected_ = true;
  if (helper_.data()->connect_data().mode == ASYNC) {
    RunCallbackAsync(callback, helper_.data()->connect_data().result);
    return ERR_IO_PENDING;
  }
  return helper_.data()->connect_data().result;
}

void DeterministicMockTCPClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
}

bool DeterministicMockTCPClientSocket::IsConnected() const {
  return connected_ && !helper_.peer_closed_connection();
}

bool DeterministicMockTCPClientSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

bool DeterministicMockTCPClientSocket::WasEverUsed() const {
  return helper_.was_used_to_convey_data();
}

bool DeterministicMockTCPClientSocket::UsingTCPFastOpen() const {
  return false;
}

bool DeterministicMockTCPClientSocket::WasNpnNegotiated() const {
  return false;
}

bool DeterministicMockTCPClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

// static
void MockSSLClientSocket::ConnectCallback(
    MockSSLClientSocket* ssl_client_socket,
    const CompletionCallback& callback,
    int rv) {
  if (rv == OK)
    ssl_client_socket->connected_ = true;
  callback.Run(rv);
}

MockSSLClientSocket::MockSSLClientSocket(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_port_pair,
    const SSLConfig& ssl_config,
    SSLSocketDataProvider* data)
    : MockClientSocket(
          // Have to use the right BoundNetLog for LoadTimingInfo regression
          // tests.
          transport_socket->socket()->NetLog()),
      transport_(transport_socket.Pass()),
      data_(data) {
  DCHECK(data_);
  peer_addr_ = data->connect.peer_addr;
}

MockSSLClientSocket::~MockSSLClientSocket() {
  Disconnect();
}

int MockSSLClientSocket::Read(IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback) {
  return transport_->socket()->Read(buf, buf_len, callback);
}

int MockSSLClientSocket::Write(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  return transport_->socket()->Write(buf, buf_len, callback);
}

int MockSSLClientSocket::Connect(const CompletionCallback& callback) {
  int rv = transport_->socket()->Connect(
      base::Bind(&ConnectCallback, base::Unretained(this), callback));
  if (rv == OK) {
    if (data_->connect.result == OK)
      connected_ = true;
    if (data_->connect.mode == ASYNC) {
      RunCallbackAsync(callback, data_->connect.result);
      return ERR_IO_PENDING;
    }
    return data_->connect.result;
  }
  return rv;
}

void MockSSLClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
  if (transport_->socket() != NULL)
    transport_->socket()->Disconnect();
}

bool MockSSLClientSocket::IsConnected() const {
  return transport_->socket()->IsConnected();
}

bool MockSSLClientSocket::WasEverUsed() const {
  return transport_->socket()->WasEverUsed();
}

bool MockSSLClientSocket::UsingTCPFastOpen() const {
  return transport_->socket()->UsingTCPFastOpen();
}

int MockSSLClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

bool MockSSLClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  ssl_info->cert = data_->cert;
  ssl_info->client_cert_sent = data_->client_cert_sent;
  ssl_info->channel_id_sent = data_->channel_id_sent;
  ssl_info->connection_status = data_->connection_status;
  return true;
}

void MockSSLClientSocket::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  DCHECK(cert_request_info);
  if (data_->cert_request_info) {
    cert_request_info->host_and_port =
        data_->cert_request_info->host_and_port;
    cert_request_info->client_certs = data_->cert_request_info->client_certs;
  } else {
    cert_request_info->Reset();
  }
}

SSLClientSocket::NextProtoStatus MockSSLClientSocket::GetNextProto(
    std::string* proto) const {
  *proto = data_->next_proto;
  return data_->next_proto_status;
}

ChannelIDService* MockSSLClientSocket::GetChannelIDService() const {
  return data_->channel_id_service;
}

void MockSSLClientSocket::OnReadComplete(const MockRead& data) {
  NOTIMPLEMENTED();
}

void MockSSLClientSocket::OnWriteComplete(int rv) {
  NOTIMPLEMENTED();
}

void MockSSLClientSocket::OnConnectComplete(const MockConnect& data) {
  NOTIMPLEMENTED();
}

MockUDPClientSocket::MockUDPClientSocket(SocketDataProvider* data,
                                         net::NetLog* net_log)
    : connected_(false),
      data_(data),
      read_offset_(0),
      read_data_(SYNCHRONOUS, ERR_UNEXPECTED),
      need_read_data_(true),
      source_port_(123),
      pending_read_buf_(NULL),
      pending_read_buf_len_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_NONE)),
      weak_factory_(this) {
  DCHECK(data_);
  data_->Reset();
  peer_addr_ = data->connect_data().peer_addr;
}

MockUDPClientSocket::~MockUDPClientSocket() {}

int MockUDPClientSocket::Read(IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  if (!connected_)
    return ERR_UNEXPECTED;

  // If the buffer is already in use, a read is already in progress!
  DCHECK(pending_read_buf_.get() == NULL);

  // Store our async IO data.
  pending_read_buf_ = buf;
  pending_read_buf_len_ = buf_len;
  pending_read_callback_ = callback;

  if (need_read_data_) {
    read_data_ = data_->OnRead();
    // ERR_IO_PENDING means that the SocketDataProvider is taking responsibility
    // to complete the async IO manually later (via OnReadComplete).
    if (read_data_.result == ERR_IO_PENDING) {
      // We need to be using async IO in this case.
      DCHECK(!callback.is_null());
      return ERR_IO_PENDING;
    }
    need_read_data_ = false;
  }

  return CompleteRead();
}

int MockUDPClientSocket::Write(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  if (!connected_)
    return ERR_UNEXPECTED;

  std::string data(buf->data(), buf_len);
  MockWriteResult write_result = data_->OnWrite(data);

  // ERR_IO_PENDING is a signal that the socket data will call back
  // asynchronously.
  if (write_result.result == ERR_IO_PENDING) {
    pending_write_callback_ = callback;
    return ERR_IO_PENDING;
  }
  if (write_result.mode == ASYNC) {
    RunCallbackAsync(callback, write_result.result);
    return ERR_IO_PENDING;
  }
  return write_result.result;
}

int MockUDPClientSocket::SetReceiveBufferSize(int32 size) {
  return OK;
}

int MockUDPClientSocket::SetSendBufferSize(int32 size) {
  return OK;
}

void MockUDPClientSocket::Close() {
  connected_ = false;
}

int MockUDPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  *address = peer_addr_;
  return OK;
}

int MockUDPClientSocket::GetLocalAddress(IPEndPoint* address) const {
  IPAddressNumber ip;
  bool rv = ParseIPLiteralToNumber("192.0.2.33", &ip);
  CHECK(rv);
  *address = IPEndPoint(ip, source_port_);
  return OK;
}

const BoundNetLog& MockUDPClientSocket::NetLog() const {
  return net_log_;
}

int MockUDPClientSocket::BindToNetwork(
    NetworkChangeNotifier::NetworkHandle network) {
  return ERR_NOT_IMPLEMENTED;
}

int MockUDPClientSocket::Connect(const IPEndPoint& address) {
  connected_ = true;
  peer_addr_ = address;
  return data_->connect_data().result;
}

void MockUDPClientSocket::OnReadComplete(const MockRead& data) {
  // There must be a read pending.
  DCHECK(pending_read_buf_.get());
  // You can't complete a read with another ERR_IO_PENDING status code.
  DCHECK_NE(ERR_IO_PENDING, data.result);
  // Since we've been waiting for data, need_read_data_ should be true.
  DCHECK(need_read_data_);

  read_data_ = data;
  need_read_data_ = false;

  // The caller is simulating that this IO completes right now.  Don't
  // let CompleteRead() schedule a callback.
  read_data_.mode = SYNCHRONOUS;

  CompletionCallback callback = pending_read_callback_;
  int rv = CompleteRead();
  RunCallback(callback, rv);
}

void MockUDPClientSocket::OnWriteComplete(int rv) {
  // There must be a read pending.
  DCHECK(!pending_write_callback_.is_null());
  CompletionCallback callback = pending_write_callback_;
  RunCallback(callback, rv);
}

void MockUDPClientSocket::OnConnectComplete(const MockConnect& data) {
  NOTIMPLEMENTED();
}

int MockUDPClientSocket::CompleteRead() {
  DCHECK(pending_read_buf_.get());
  DCHECK(pending_read_buf_len_ > 0);

  // Save the pending async IO data and reset our |pending_| state.
  scoped_refptr<IOBuffer> buf = pending_read_buf_;
  int buf_len = pending_read_buf_len_;
  CompletionCallback callback = pending_read_callback_;
  pending_read_buf_ = NULL;
  pending_read_buf_len_ = 0;
  pending_read_callback_.Reset();

  int result = read_data_.result;
  DCHECK(result != ERR_IO_PENDING);

  if (read_data_.data) {
    if (read_data_.data_len - read_offset_ > 0) {
      result = std::min(buf_len, read_data_.data_len - read_offset_);
      memcpy(buf->data(), read_data_.data + read_offset_, result);
      read_offset_ += result;
      if (read_offset_ == read_data_.data_len) {
        need_read_data_ = true;
        read_offset_ = 0;
      }
    } else {
      result = 0;  // EOF
    }
  }

  if (read_data_.mode == ASYNC) {
    DCHECK(!callback.is_null());
    RunCallbackAsync(callback, result);
    return ERR_IO_PENDING;
  }
  return result;
}

void MockUDPClientSocket::RunCallbackAsync(const CompletionCallback& callback,
                                           int result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&MockUDPClientSocket::RunCallback,
                            weak_factory_.GetWeakPtr(), callback, result));
}

void MockUDPClientSocket::RunCallback(const CompletionCallback& callback,
                                      int result) {
  if (!callback.is_null())
    callback.Run(result);
}

TestSocketRequest::TestSocketRequest(
    std::vector<TestSocketRequest*>* request_order, size_t* completion_count)
    : request_order_(request_order),
      completion_count_(completion_count),
      callback_(base::Bind(&TestSocketRequest::OnComplete,
                           base::Unretained(this))) {
  DCHECK(request_order);
  DCHECK(completion_count);
}

TestSocketRequest::~TestSocketRequest() {
}

void TestSocketRequest::OnComplete(int result) {
  SetResult(result);
  (*completion_count_)++;
  request_order_->push_back(this);
}

// static
const int ClientSocketPoolTest::kIndexOutOfBounds = -1;

// static
const int ClientSocketPoolTest::kRequestNotFound = -2;

ClientSocketPoolTest::ClientSocketPoolTest() : completion_count_(0) {}
ClientSocketPoolTest::~ClientSocketPoolTest() {}

int ClientSocketPoolTest::GetOrderOfRequest(size_t index) const {
  index--;
  if (index >= requests_.size())
    return kIndexOutOfBounds;

  for (size_t i = 0; i < request_order_.size(); i++)
    if (requests_[index] == request_order_[i])
      return i + 1;

  return kRequestNotFound;
}

bool ClientSocketPoolTest::ReleaseOneConnection(KeepAlive keep_alive) {
  ScopedVector<TestSocketRequest>::iterator i;
  for (i = requests_.begin(); i != requests_.end(); ++i) {
    if ((*i)->handle()->is_initialized()) {
      if (keep_alive == NO_KEEP_ALIVE)
        (*i)->handle()->socket()->Disconnect();
      (*i)->handle()->Reset();
      base::RunLoop().RunUntilIdle();
      return true;
    }
  }
  return false;
}

void ClientSocketPoolTest::ReleaseAllConnections(KeepAlive keep_alive) {
  bool released_one;
  do {
    released_one = ReleaseOneConnection(keep_alive);
  } while (released_one);
}

MockTransportClientSocketPool::MockConnectJob::MockConnectJob(
    scoped_ptr<StreamSocket> socket,
    ClientSocketHandle* handle,
    const CompletionCallback& callback)
    : socket_(socket.Pass()),
      handle_(handle),
      user_callback_(callback) {
}

MockTransportClientSocketPool::MockConnectJob::~MockConnectJob() {}

int MockTransportClientSocketPool::MockConnectJob::Connect() {
  int rv = socket_->Connect(base::Bind(&MockConnectJob::OnConnect,
                                       base::Unretained(this)));
  if (rv != ERR_IO_PENDING) {
    user_callback_.Reset();
    OnConnect(rv);
  }
  return rv;
}

bool MockTransportClientSocketPool::MockConnectJob::CancelHandle(
    const ClientSocketHandle* handle) {
  if (handle != handle_)
    return false;
  socket_.reset();
  handle_ = NULL;
  user_callback_.Reset();
  return true;
}

void MockTransportClientSocketPool::MockConnectJob::OnConnect(int rv) {
  if (!socket_.get())
    return;
  if (rv == OK) {
    handle_->SetSocket(socket_.Pass());

    // Needed for socket pool tests that layer other sockets on top of mock
    // sockets.
    LoadTimingInfo::ConnectTiming connect_timing;
    base::TimeTicks now = base::TimeTicks::Now();
    connect_timing.dns_start = now;
    connect_timing.dns_end = now;
    connect_timing.connect_start = now;
    connect_timing.connect_end = now;
    handle_->set_connect_timing(connect_timing);
  } else {
    socket_.reset();

    // Needed to test copying of ConnectionAttempts in SSL ConnectJob.
    ConnectionAttempts attempts;
    attempts.push_back(ConnectionAttempt(IPEndPoint(), rv));
    handle_->set_connection_attempts(attempts);
  }

  handle_ = NULL;

  if (!user_callback_.is_null()) {
    CompletionCallback callback = user_callback_;
    user_callback_.Reset();
    callback.Run(rv);
  }
}

MockTransportClientSocketPool::MockTransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketFactory* socket_factory)
    : TransportClientSocketPool(max_sockets,
                                max_sockets_per_group,
                                NULL,
                                NULL,
                                NULL),
      client_socket_factory_(socket_factory),
      last_request_priority_(DEFAULT_PRIORITY),
      release_count_(0),
      cancel_count_(0) {
}

MockTransportClientSocketPool::~MockTransportClientSocketPool() {}

int MockTransportClientSocketPool::RequestSocket(
    const std::string& group_name, const void* socket_params,
    RequestPriority priority, ClientSocketHandle* handle,
    const CompletionCallback& callback, const BoundNetLog& net_log) {
  last_request_priority_ = priority;
  scoped_ptr<StreamSocket> socket =
      client_socket_factory_->CreateTransportClientSocket(
          AddressList(), net_log.net_log(), NetLog::Source());
  MockConnectJob* job = new MockConnectJob(socket.Pass(), handle, callback);
  job_list_.push_back(job);
  handle->set_pool_id(1);
  return job->Connect();
}

void MockTransportClientSocketPool::CancelRequest(const std::string& group_name,
                                                  ClientSocketHandle* handle) {
  std::vector<MockConnectJob*>::iterator i;
  for (i = job_list_.begin(); i != job_list_.end(); ++i) {
    if ((*i)->CancelHandle(handle)) {
      cancel_count_++;
      break;
    }
  }
}

void MockTransportClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    scoped_ptr<StreamSocket> socket,
    int id) {
  EXPECT_EQ(1, id);
  release_count_++;
}

DeterministicMockClientSocketFactory::DeterministicMockClientSocketFactory() {}

DeterministicMockClientSocketFactory::~DeterministicMockClientSocketFactory() {}

void DeterministicMockClientSocketFactory::AddSocketDataProvider(
    DeterministicSocketData* data) {
  mock_data_.Add(data);
}

void DeterministicMockClientSocketFactory::AddSSLSocketDataProvider(
    SSLSocketDataProvider* data) {
  mock_ssl_data_.Add(data);
}

void DeterministicMockClientSocketFactory::ResetNextMockIndexes() {
  mock_data_.ResetNextIndex();
  mock_ssl_data_.ResetNextIndex();
}

MockSSLClientSocket* DeterministicMockClientSocketFactory::
    GetMockSSLClientSocket(size_t index) const {
  DCHECK_LT(index, ssl_client_sockets_.size());
  return ssl_client_sockets_[index];
}

scoped_ptr<DatagramClientSocket>
DeterministicMockClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    const RandIntCallback& rand_int_cb,
    NetLog* net_log,
    const NetLog::Source& source) {
  DeterministicSocketData* data_provider = mock_data().GetNext();
  scoped_ptr<DeterministicMockUDPClientSocket> socket(
      new DeterministicMockUDPClientSocket(net_log, data_provider));
  data_provider->set_delegate(socket->AsWeakPtr());
  udp_client_sockets().push_back(socket.get());
  if (bind_type == DatagramSocket::RANDOM_BIND)
    socket->set_source_port(static_cast<uint16>(rand_int_cb.Run(1025, 65535)));
  return socket.Pass();
}

scoped_ptr<StreamSocket>
DeterministicMockClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    NetLog* net_log,
    const NetLog::Source& source) {
  DeterministicSocketData* data_provider = mock_data().GetNext();
  scoped_ptr<DeterministicMockTCPClientSocket> socket(
      new DeterministicMockTCPClientSocket(net_log, data_provider));
  data_provider->set_delegate(socket->AsWeakPtr());
  tcp_client_sockets().push_back(socket.get());
  return socket.Pass();
}

scoped_ptr<SSLClientSocket>
DeterministicMockClientSocketFactory::CreateSSLClientSocket(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context) {
  scoped_ptr<MockSSLClientSocket> socket(
      new MockSSLClientSocket(transport_socket.Pass(),
                              host_and_port, ssl_config,
                              mock_ssl_data_.GetNext()));
  ssl_client_sockets_.push_back(socket.get());
  return socket.Pass();
}

void DeterministicMockClientSocketFactory::ClearSSLSessionCache() {
}

MockSOCKSClientSocketPool::MockSOCKSClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    TransportClientSocketPool* transport_pool)
    : SOCKSClientSocketPool(max_sockets,
                            max_sockets_per_group,
                            NULL,
                            transport_pool,
                            NULL),
      transport_pool_(transport_pool) {
}

MockSOCKSClientSocketPool::~MockSOCKSClientSocketPool() {}

int MockSOCKSClientSocketPool::RequestSocket(
    const std::string& group_name, const void* socket_params,
    RequestPriority priority, ClientSocketHandle* handle,
    const CompletionCallback& callback, const BoundNetLog& net_log) {
  return transport_pool_->RequestSocket(
      group_name, socket_params, priority, handle, callback, net_log);
}

void MockSOCKSClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  return transport_pool_->CancelRequest(group_name, handle);
}

void MockSOCKSClientSocketPool::ReleaseSocket(const std::string& group_name,
                                              scoped_ptr<StreamSocket> socket,
                                              int id) {
  return transport_pool_->ReleaseSocket(group_name, socket.Pass(), id);
}

ScopedWebSocketEndpointZeroUnlockDelay::
    ScopedWebSocketEndpointZeroUnlockDelay() {
  old_delay_ =
      WebSocketEndpointLockManager::GetInstance()->SetUnlockDelayForTesting(
          base::TimeDelta());
}

ScopedWebSocketEndpointZeroUnlockDelay::
    ~ScopedWebSocketEndpointZeroUnlockDelay() {
  base::TimeDelta active_delay =
      WebSocketEndpointLockManager::GetInstance()->SetUnlockDelayForTesting(
          old_delay_);
  EXPECT_EQ(active_delay, base::TimeDelta());
}

const char kSOCKS5GreetRequest[] = { 0x05, 0x01, 0x00 };
const int kSOCKS5GreetRequestLength = arraysize(kSOCKS5GreetRequest);

const char kSOCKS5GreetResponse[] = { 0x05, 0x00 };
const int kSOCKS5GreetResponseLength = arraysize(kSOCKS5GreetResponse);

const char kSOCKS5OkRequest[] =
    { 0x05, 0x01, 0x00, 0x03, 0x04, 'h', 'o', 's', 't', 0x00, 0x50 };
const int kSOCKS5OkRequestLength = arraysize(kSOCKS5OkRequest);

const char kSOCKS5OkResponse[] =
    { 0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0x00, 0x50 };
const int kSOCKS5OkResponseLength = arraysize(kSOCKS5OkResponse);

int64_t CountReadBytes(const MockRead reads[], size_t reads_size) {
  int64_t total = 0;
  for (const MockRead* read = reads; read != reads + reads_size; ++read)
    total += read->data_len;
  return total;
}

int64_t CountWriteBytes(const MockWrite writes[], size_t writes_size) {
  int64_t total = 0;
  for (const MockWrite* write = writes; write != writes + writes_size; ++write)
    total += write->data_len;
  return total;
}

}  // namespace net
