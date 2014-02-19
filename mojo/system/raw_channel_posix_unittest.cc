// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vtl): Factor out the remaining POSIX-specific bits of this test (once we
// have a non-POSIX implementation).

#include "mojo/system/raw_channel.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "mojo/system/embedder/platform_channel_pair.h"
#include "mojo/system/embedder/platform_handle.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/test_utils.h"

namespace mojo {
namespace system {
namespace {

MessageInTransit* MakeTestMessage(uint32_t num_bytes) {
  std::vector<unsigned char> bytes(num_bytes, 0);
  for (size_t i = 0; i < num_bytes; i++)
    bytes[i] = static_cast<unsigned char>(i + num_bytes);
  return MessageInTransit::Create(
      MessageInTransit::kTypeMessagePipeEndpoint,
      MessageInTransit::kSubtypeMessagePipeEndpointData,
      bytes.data(), num_bytes, 0);
}

bool CheckMessageData(const void* bytes, uint32_t num_bytes) {
  const unsigned char* b = static_cast<const unsigned char*>(bytes);
  for (uint32_t i = 0; i < num_bytes; i++) {
    if (b[i] != static_cast<unsigned char>(i + num_bytes))
      return false;
  }
  return true;
}

void InitOnIOThread(RawChannel* raw_channel) {
  CHECK(raw_channel->Init());
}

bool WriteTestMessageToHandle(const embedder::PlatformHandle& handle,
                              uint32_t num_bytes) {
  MessageInTransit* message = MakeTestMessage(num_bytes);

  ssize_t write_size = HANDLE_EINTR(
     write(handle.fd, message->main_buffer(), message->main_buffer_size()));
  bool result = write_size == static_cast<ssize_t>(message->main_buffer_size());
  message->Destroy();
  return result;
}

// -----------------------------------------------------------------------------

class RawChannelPosixTest : public test::TestWithIOThreadBase {
 public:
  RawChannelPosixTest() {}
  virtual ~RawChannelPosixTest() {}

  virtual void SetUp() OVERRIDE {
    test::TestWithIOThreadBase::SetUp();

    embedder::PlatformChannelPair channel_pair;
    handles[0] = channel_pair.PassServerHandle();
    handles[1] = channel_pair.PassClientHandle();
  }

  virtual void TearDown() OVERRIDE {
    handles[0].reset();
    handles[1].reset();

    test::TestWithIOThreadBase::TearDown();
  }

 protected:
  embedder::ScopedPlatformHandle handles[2];

 private:
  DISALLOW_COPY_AND_ASSIGN(RawChannelPosixTest);
};

// RawChannelPosixTest.WriteMessage --------------------------------------------

class WriteOnlyRawChannelDelegate : public RawChannel::Delegate {
 public:
  WriteOnlyRawChannelDelegate() {}
  virtual ~WriteOnlyRawChannelDelegate() {}

  // |RawChannel::Delegate| implementation:
  virtual void OnReadMessage(const MessageInTransit& /*message*/) OVERRIDE {
    NOTREACHED();
  }
  virtual void OnFatalError(FatalError /*fatal_error*/) OVERRIDE {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WriteOnlyRawChannelDelegate);
};

static const int64_t kMessageReaderSleepMs = 1;
static const size_t kMessageReaderMaxPollIterations = 3000;

class TestMessageReaderAndChecker {
 public:
  explicit TestMessageReaderAndChecker(embedder::PlatformHandle handle)
      : handle_(handle) {}
  ~TestMessageReaderAndChecker() { CHECK(bytes_.empty()); }

  bool ReadAndCheckNextMessage(uint32_t expected_size) {
    unsigned char buffer[4096];

    for (size_t i = 0; i < kMessageReaderMaxPollIterations;) {
      ssize_t read_size = HANDLE_EINTR(
          read(handle_.fd, buffer, sizeof(buffer)));
      if (read_size < 0) {
        PCHECK(errno == EAGAIN || errno == EWOULDBLOCK);
        read_size = 0;
      }

      // Append newly-read data to |bytes_|.
      bytes_.insert(bytes_.end(), buffer, buffer + read_size);

      // If we have the header....
      if (bytes_.size() >= sizeof(MessageInTransit)) {
        const MessageInTransit* message =
            reinterpret_cast<const MessageInTransit*>(bytes_.data());
        CHECK_EQ(reinterpret_cast<size_t>(message) %
                     MessageInTransit::kMessageAlignment, 0u);

        if (message->data_size() != expected_size) {
          LOG(ERROR) << "Wrong size: " << message->data_size() << " instead of "
                     << expected_size << " bytes.";
          return false;
        }

        // If we've read the whole message....
        if (bytes_.size() >= message->main_buffer_size()) {
          if (!CheckMessageData(message->data(), message->data_size())) {
            LOG(ERROR) << "Incorrect message data.";
            return false;
          }

          // Erase message data.
          bytes_.erase(bytes_.begin(),
                       bytes_.begin() +
                           message->main_buffer_size());
          return true;
        }
      }

      if (static_cast<size_t>(read_size) < sizeof(buffer)) {
        i++;
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kMessageReaderSleepMs));
      }
    }

    LOG(ERROR) << "Too many iterations.";
    return false;
  }

 private:
  const embedder::PlatformHandle handle_;

  // The start of the received data should always be on a message boundary.
  std::vector<unsigned char> bytes_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageReaderAndChecker);
};

// Tests writing (and verifies reading using our own custom reader).
TEST_F(RawChannelPosixTest, WriteMessage) {
  WriteOnlyRawChannelDelegate delegate;
  scoped_ptr<RawChannel> rc(RawChannel::Create(handles[0].Pass(),
                                               &delegate,
                                               io_thread_message_loop()));

  TestMessageReaderAndChecker checker(handles[1].get());

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&InitOnIOThread, rc.get()));

  // Write and read, for a variety of sizes.
  for (uint32_t size = 1; size < 5 * 1000 * 1000; size += size / 2 + 1) {
    EXPECT_TRUE(rc->WriteMessage(MakeTestMessage(size)));
    EXPECT_TRUE(checker.ReadAndCheckNextMessage(size)) << size;
  }

  // Write/queue and read afterwards, for a variety of sizes.
  for (uint32_t size = 1; size < 5 * 1000 * 1000; size += size / 2 + 1)
    EXPECT_TRUE(rc->WriteMessage(MakeTestMessage(size)));
  for (uint32_t size = 1; size < 5 * 1000 * 1000; size += size / 2 + 1)
    EXPECT_TRUE(checker.ReadAndCheckNextMessage(size)) << size;

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&RawChannel::Shutdown,
                                   base::Unretained(rc.get())));
}

// RawChannelPosixTest.OnReadMessage -------------------------------------------

class ReadCheckerRawChannelDelegate : public RawChannel::Delegate {
 public:
  ReadCheckerRawChannelDelegate()
      : done_event_(false, false),
        position_(0) {}
  virtual ~ReadCheckerRawChannelDelegate() {}

  // |RawChannel::Delegate| implementation (called on the I/O thread):
  virtual void OnReadMessage(const MessageInTransit& message) OVERRIDE {
    size_t position;
    size_t expected_size;
    bool should_signal = false;
    {
      base::AutoLock locker(lock_);
      CHECK_LT(position_, expected_sizes_.size());
      position = position_;
      expected_size = expected_sizes_[position];
      position_++;
      if (position_ >= expected_sizes_.size())
        should_signal = true;
    }

    EXPECT_EQ(expected_size, message.data_size()) << position;
    if (message.data_size() == expected_size) {
      EXPECT_TRUE(CheckMessageData(message.data(), message.data_size()))
          << position;
    }

    if (should_signal)
      done_event_.Signal();
  }
  virtual void OnFatalError(FatalError /*fatal_error*/) OVERRIDE {
    NOTREACHED();
  }

  // Wait for all the messages (of sizes |expected_sizes_|) to be seen.
  void Wait() {
    done_event_.Wait();
  }

  void SetExpectedSizes(const std::vector<uint32_t>& expected_sizes) {
    base::AutoLock locker(lock_);
    CHECK_EQ(position_, expected_sizes_.size());
    expected_sizes_ = expected_sizes;
    position_ = 0;
  }

 private:
  base::WaitableEvent done_event_;

  base::Lock lock_;  // Protects the following members.
  std::vector<uint32_t> expected_sizes_;
  size_t position_;

  DISALLOW_COPY_AND_ASSIGN(ReadCheckerRawChannelDelegate);
};

// Tests reading (writing using our own custom writer).
TEST_F(RawChannelPosixTest, OnReadMessage) {
  // We're going to write to |fd(1)|. We'll do so in a blocking manner.
  PCHECK(fcntl(handles[1].get().fd, F_SETFL, 0) == 0);

  ReadCheckerRawChannelDelegate delegate;
  scoped_ptr<RawChannel> rc(RawChannel::Create(handles[0].Pass(),
                                               &delegate,
                                               io_thread_message_loop()));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&InitOnIOThread, rc.get()));

  // Write and read, for a variety of sizes.
  for (uint32_t size = 1; size < 5 * 1000 * 1000; size += size / 2 + 1) {
    delegate.SetExpectedSizes(std::vector<uint32_t>(1, size));

    EXPECT_TRUE(WriteTestMessageToHandle(handles[1].get(), size));

    delegate.Wait();
  }

  // Set up reader and write as fast as we can.
  // Write/queue and read afterwards, for a variety of sizes.
  std::vector<uint32_t> expected_sizes;
  for (uint32_t size = 1; size < 5 * 1000 * 1000; size += size / 2 + 1)
    expected_sizes.push_back(size);
  delegate.SetExpectedSizes(expected_sizes);
  for (uint32_t size = 1; size < 5 * 1000 * 1000; size += size / 2 + 1)
    EXPECT_TRUE(WriteTestMessageToHandle(handles[1].get(), size));
  delegate.Wait();

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&RawChannel::Shutdown,
                                   base::Unretained(rc.get())));
}

// RawChannelPosixTest.WriteMessageAndOnReadMessage ----------------------------

class RawChannelWriterThread : public base::SimpleThread {
 public:
  RawChannelWriterThread(RawChannel* raw_channel, size_t write_count)
      : base::SimpleThread("raw_channel_writer_thread"),
        raw_channel_(raw_channel),
        left_to_write_(write_count) {
  }

  virtual ~RawChannelWriterThread() {
    Join();
  }

 private:
  virtual void Run() OVERRIDE {
    static const int kMaxRandomMessageSize = 25000;

    while (left_to_write_-- > 0) {
      EXPECT_TRUE(raw_channel_->WriteMessage(MakeTestMessage(
          static_cast<uint32_t>(base::RandInt(1, kMaxRandomMessageSize)))));
    }
  }

  RawChannel* const raw_channel_;
  size_t left_to_write_;

  DISALLOW_COPY_AND_ASSIGN(RawChannelWriterThread);
};

class ReadCountdownRawChannelDelegate : public RawChannel::Delegate {
 public:
  explicit ReadCountdownRawChannelDelegate(size_t expected_count)
      : done_event_(false, false),
        expected_count_(expected_count),
        count_(0) {}
  virtual ~ReadCountdownRawChannelDelegate() {}

  // |RawChannel::Delegate| implementation (called on the I/O thread):
  virtual void OnReadMessage(const MessageInTransit& message) OVERRIDE {
    EXPECT_LT(count_, expected_count_);
    count_++;

    EXPECT_TRUE(CheckMessageData(message.data(), message.data_size()));

    if (count_ >= expected_count_)
      done_event_.Signal();
  }
  virtual void OnFatalError(FatalError /*fatal_error*/) OVERRIDE {
    NOTREACHED();
  }

  // Wait for all the messages to have been seen.
  void Wait() {
    done_event_.Wait();
  }

 private:
  base::WaitableEvent done_event_;
  size_t expected_count_;
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(ReadCountdownRawChannelDelegate);
};

TEST_F(RawChannelPosixTest, WriteMessageAndOnReadMessage) {
  static const size_t kNumWriterThreads = 10;
  static const size_t kNumWriteMessagesPerThread = 4000;

  WriteOnlyRawChannelDelegate writer_delegate;
  scoped_ptr<RawChannel> writer_rc(
      RawChannel::Create(handles[0].Pass(),
                         &writer_delegate,
                         io_thread_message_loop()));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&InitOnIOThread, writer_rc.get()));

  ReadCountdownRawChannelDelegate reader_delegate(
      kNumWriterThreads * kNumWriteMessagesPerThread);
  scoped_ptr<RawChannel> reader_rc(
      RawChannel::Create(handles[1].Pass(),
                         &reader_delegate,
                         io_thread_message_loop()));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&InitOnIOThread, reader_rc.get()));

  {
    ScopedVector<RawChannelWriterThread> writer_threads;
    for (size_t i = 0; i < kNumWriterThreads; i++) {
      writer_threads.push_back(new RawChannelWriterThread(
          writer_rc.get(), kNumWriteMessagesPerThread));
    }
    for (size_t i = 0; i < writer_threads.size(); i++)
      writer_threads[i]->Start();
  }  // Joins all the writer threads.

  // Sleep a bit, to let any extraneous reads be processed. (There shouldn't be
  // any, but we want to know about them.)
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));

  // Wait for reading to finish.
  reader_delegate.Wait();

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&RawChannel::Shutdown,
                                   base::Unretained(reader_rc.get())));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&RawChannel::Shutdown,
                                   base::Unretained(writer_rc.get())));
}

// RawChannelPosixTest.OnFatalError --------------------------------------------

class FatalErrorRecordingRawChannelDelegate
    : public ReadCountdownRawChannelDelegate {
 public:
  FatalErrorRecordingRawChannelDelegate(size_t expected_read_count_)
      : ReadCountdownRawChannelDelegate(expected_read_count_),
        got_fatal_error_event_(false, false),
        on_fatal_error_call_count_(0),
        last_fatal_error_(FATAL_ERROR_UNKNOWN) {}

  virtual ~FatalErrorRecordingRawChannelDelegate() {}

  virtual void OnFatalError(FatalError fatal_error) OVERRIDE {
    CHECK_EQ(on_fatal_error_call_count_, 0);
    on_fatal_error_call_count_++;
    last_fatal_error_ = fatal_error;
    got_fatal_error_event_.Signal();
  }

  FatalError WaitForFatalError() {
    got_fatal_error_event_.Wait();
    CHECK_EQ(on_fatal_error_call_count_, 1);
    return last_fatal_error_;
  }

 private:
  base::WaitableEvent got_fatal_error_event_;

  int on_fatal_error_call_count_;
  FatalError last_fatal_error_;

  DISALLOW_COPY_AND_ASSIGN(FatalErrorRecordingRawChannelDelegate);
};

// Tests fatal errors.
// TODO(vtl): Figure out how to make reading fail reliably. (I'm not convinced
// that it does.)
TEST_F(RawChannelPosixTest, OnFatalError) {
  const size_t kMessageCount = 5;

  // We're going to write to |fd(1)|. We'll do so in a blocking manner.
  PCHECK(fcntl(handles[1].get().fd, F_SETFL, 0) == 0);

  FatalErrorRecordingRawChannelDelegate delegate(2 * kMessageCount);
  scoped_ptr<RawChannel> rc(RawChannel::Create(handles[0].Pass(),
                                               &delegate,
                                               io_thread_message_loop()));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&InitOnIOThread, rc.get()));

  // Write into the other end a few messages.
  uint32_t message_size = 1;
  for (size_t count = 0; count < kMessageCount;
       ++count, message_size += message_size / 2 + 1) {
    EXPECT_TRUE(WriteTestMessageToHandle(handles[1].get(), message_size));
  }

  // Shut down read at the other end, which should make writing fail.
  EXPECT_EQ(0, shutdown(handles[1].get().fd, SHUT_RD));

  EXPECT_FALSE(rc->WriteMessage(MakeTestMessage(1)));

  // TODO(vtl): In theory, it's conceivable that closing the other end might
  // lead to read failing. In practice, it doesn't seem to.
  EXPECT_EQ(RawChannel::Delegate::FATAL_ERROR_FAILED_WRITE,
            delegate.WaitForFatalError());

  EXPECT_FALSE(rc->WriteMessage(MakeTestMessage(2)));

  // Sleep a bit, to make sure we don't get another |OnFatalError()|
  // notification. (If we actually get another one, |OnFatalError()| crashes.)
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));

  // Write into the other end a few more messages.
  for (size_t count = 0; count < kMessageCount;
       ++count, message_size += message_size / 2 + 1) {
    EXPECT_TRUE(WriteTestMessageToHandle(handles[1].get(), message_size));
  }
  // Wait for reading to finish. A writing failure shouldn't affect reading.
  delegate.Wait();

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&RawChannel::Shutdown,
                                   base::Unretained(rc.get())));
}

// RawChannelPosixTest.WriteMessageAfterShutdown -------------------------------

// Makes sure that calling |WriteMessage()| after |Shutdown()| behaves
// correctly.
TEST_F(RawChannelPosixTest, WriteMessageAfterShutdown) {
  WriteOnlyRawChannelDelegate delegate;
  scoped_ptr<RawChannel> rc(RawChannel::Create(handles[0].Pass(),
                                               &delegate,
                                               io_thread_message_loop()));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&InitOnIOThread, rc.get()));
  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&RawChannel::Shutdown,
                                   base::Unretained(rc.get())));

  EXPECT_FALSE(rc->WriteMessage(MakeTestMessage(1)));
}

}  // namespace
}  // namespace system
}  // namespace mojo
