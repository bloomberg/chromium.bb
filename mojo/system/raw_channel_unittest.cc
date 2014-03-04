// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_channel.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/system/embedder/platform_channel_pair.h"
#include "mojo/system/embedder/platform_handle.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/test_utils.h"

#if defined(OS_POSIX)
#include <sys/socket.h>
#endif

namespace mojo {
namespace system {
namespace {

scoped_ptr<MessageInTransit> MakeTestMessage(uint32_t num_bytes) {
  std::vector<unsigned char> bytes(num_bytes, 0);
  for (size_t i = 0; i < num_bytes; i++)
    bytes[i] = static_cast<unsigned char>(i + num_bytes);
  return make_scoped_ptr(
      new MessageInTransit(MessageInTransit::kTypeMessagePipeEndpoint,
                           MessageInTransit::kSubtypeMessagePipeEndpointData,
                           num_bytes, 0, bytes.data()));
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
  scoped_ptr<MessageInTransit> message(MakeTestMessage(num_bytes));

  size_t write_size = 0;
  mojo::test::BlockingWrite(
      handle, message->main_buffer(), message->main_buffer_size(), &write_size);
  return write_size == message->main_buffer_size();
}

// -----------------------------------------------------------------------------

class RawChannelTest : public test::TestWithIOThreadBase {
 public:
  RawChannelTest() {}
  virtual ~RawChannelTest() {}

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
  DISALLOW_COPY_AND_ASSIGN(RawChannelTest);
};

// RawChannelTest.WriteMessage -------------------------------------------------

class WriteOnlyRawChannelDelegate : public RawChannel::Delegate {
 public:
  WriteOnlyRawChannelDelegate() {}
  virtual ~WriteOnlyRawChannelDelegate() {}

  // |RawChannel::Delegate| implementation:
  virtual void OnReadMessage(
      const MessageInTransit::View& /*message_view*/) OVERRIDE {
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
      size_t read_size = 0;
      CHECK(mojo::test::NonBlockingRead(handle_, buffer, sizeof(buffer),
                                        &read_size));

      // Append newly-read data to |bytes_|.
      bytes_.insert(bytes_.end(), buffer, buffer + read_size);

      // If we have the header....
      size_t message_size;
      if (MessageInTransit::GetNextMessageSize(bytes_.data(), bytes_.size(),
                                               &message_size)) {
        // If we've read the whole message....
        if (bytes_.size() >= message_size) {
          bool rv = true;
          MessageInTransit::View message_view(message_size, bytes_.data());
          CHECK_EQ(message_view.main_buffer_size(), message_size);

          if (message_view.num_bytes() != expected_size) {
            LOG(ERROR) << "Wrong size: " << message_size << " instead of "
                       << expected_size << " bytes.";
            rv = false;
          } else if (!CheckMessageData(message_view.bytes(),
                                       message_view.num_bytes())) {
            LOG(ERROR) << "Incorrect message bytes.";
            rv = false;
          }

          // Erase message data.
          bytes_.erase(bytes_.begin(),
                       bytes_.begin() +
                           message_view.main_buffer_size());
          return rv;
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
TEST_F(RawChannelTest, WriteMessage) {
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

// RawChannelTest.OnReadMessage ------------------------------------------------

class ReadCheckerRawChannelDelegate : public RawChannel::Delegate {
 public:
  ReadCheckerRawChannelDelegate()
      : done_event_(false, false),
        position_(0) {}
  virtual ~ReadCheckerRawChannelDelegate() {}

  // |RawChannel::Delegate| implementation (called on the I/O thread):
  virtual void OnReadMessage(
      const MessageInTransit::View& message_view) OVERRIDE {
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

    EXPECT_EQ(expected_size, message_view.num_bytes()) << position;
    if (message_view.num_bytes() == expected_size) {
      EXPECT_TRUE(CheckMessageData(message_view.bytes(),
                  message_view.num_bytes())) << position;
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
TEST_F(RawChannelTest, OnReadMessage) {
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

// RawChannelTest.WriteMessageAndOnReadMessage ---------------------------------

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
  virtual void OnReadMessage(
      const MessageInTransit::View& message_view) OVERRIDE {
    EXPECT_LT(count_, expected_count_);
    count_++;

    EXPECT_TRUE(CheckMessageData(message_view.bytes(),
                message_view.num_bytes()));

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

TEST_F(RawChannelTest, WriteMessageAndOnReadMessage) {
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

// RawChannelTest.OnFatalError -------------------------------------------------

class FatalErrorRecordingRawChannelDelegate
    : public ReadCountdownRawChannelDelegate {
 public:
  FatalErrorRecordingRawChannelDelegate(size_t expected_read_count,
                                        bool expect_read_error,
                                        bool expect_write_error)
      : ReadCountdownRawChannelDelegate(expected_read_count),
        got_fatal_error_event_(false, false),
        expecting_read_error_(expect_read_error),
        expecting_write_error_(expect_write_error) {
  }

  virtual ~FatalErrorRecordingRawChannelDelegate() {}

  virtual void OnFatalError(FatalError fatal_error) OVERRIDE {
    if (fatal_error == FATAL_ERROR_FAILED_READ) {
      ASSERT_TRUE(expecting_read_error_);
      expecting_read_error_ = false;
    } else if (fatal_error == FATAL_ERROR_FAILED_WRITE) {
      ASSERT_TRUE(expecting_write_error_);
      expecting_write_error_ = false;
    } else {
      ASSERT_TRUE(false);
    }

    if (!expecting_read_error_ && !expecting_write_error_)
      got_fatal_error_event_.Signal();
  }

  void WaitForFatalError() {
    got_fatal_error_event_.Wait();
  }

 private:
  base::WaitableEvent got_fatal_error_event_;

  bool expecting_read_error_;
  bool expecting_write_error_;

  DISALLOW_COPY_AND_ASSIGN(FatalErrorRecordingRawChannelDelegate);
};

// Tests fatal errors.
// TODO(vtl): Figure out how to make reading fail reliably. (I'm not convinced
// that it does.)
TEST_F(RawChannelTest, OnFatalError) {
  FatalErrorRecordingRawChannelDelegate delegate(0, false, true);

  scoped_ptr<RawChannel> rc(RawChannel::Create(handles[0].Pass(),
                                               &delegate,
                                               io_thread_message_loop()));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&InitOnIOThread, rc.get()));

  // Close the handle of the other end, which should make writing fail.
  handles[1].reset();

  EXPECT_FALSE(rc->WriteMessage(MakeTestMessage(1)));

  // TODO(vtl): In theory, it's conceivable that closing the other end might
  // lead to read failing. In practice, it doesn't seem to.
  delegate.WaitForFatalError();

  EXPECT_FALSE(rc->WriteMessage(MakeTestMessage(2)));

  // Sleep a bit, to make sure we don't get another |OnFatalError()|
  // notification. (If we actually get another one, |OnFatalError()| crashes.)
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));

  test::PostTaskAndWait(io_thread_task_runner(),
                        FROM_HERE,
                        base::Bind(&RawChannel::Shutdown,
                                   base::Unretained(rc.get())));
}

#if defined(OS_POSIX)
// RawChannelTest.ReadUnaffectedByWriteFatalError ------------------------------

// TODO(yzshen): On Windows, I haven't figured out a way to shut down one
// direction of the named pipe.
TEST_F(RawChannelTest, ReadUnaffectedByWriteFatalError) {
  const size_t kMessageCount = 5;

  FatalErrorRecordingRawChannelDelegate delegate(2 * kMessageCount, false,
                                                 true);
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

  delegate.WaitForFatalError();

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
#endif  // defined(OS_POSIX)

// RawChannelTest.WriteMessageAfterShutdown ------------------------------------

// Makes sure that calling |WriteMessage()| after |Shutdown()| behaves
// correctly.
TEST_F(RawChannelTest, WriteMessageAfterShutdown) {
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
