// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/raw_channel.h"

#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <deque>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/transport_data.h"
#include "mojo/public/cpp/system/macros.h"

#if !defined(SO_PEEK_OFF)
#define SO_PEEK_OFF 42
#endif

namespace mojo {
namespace edk {

namespace {

class RawChannelPosix final : public RawChannel,
                              public base::MessageLoopForIO::Watcher {
 public:
  explicit RawChannelPosix(ScopedPlatformHandle handle);
  ~RawChannelPosix() override;

  PlatformHandle GetFD() { return fd_.get(); }

 private:
  // |RawChannel| protected methods:
  // Actually override this so that we can send multiple messages with (only)
  // FDs if necessary.
  void EnqueueMessageNoLock(scoped_ptr<MessageInTransit> message) override;
  // Override this to handle those extra FD-only messages.
  bool OnReadMessageForRawChannel(
      const MessageInTransit::View& message_view) override;

  ScopedPlatformHandle ReleaseHandleNoLock(
      std::vector<char>* serialized_read_buffer,
      std::vector<char>* serialized_write_buffer,
      std::vector<int>* serialized_read_fds,
      std::vector<int>* serialized_write_fds,
      bool* write_error) override;
  void SetSerializedFDs(std::vector<int>* serialized_read_fds,
                        std::vector<int>* serialized_write_fds) override;
  bool IsHandleValid() override;
  IOResult Read(size_t* bytes_read) override;
  IOResult ScheduleRead() override;
  ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_platform_handles,
      const void* platform_handle_table) override;
  size_t SerializePlatformHandles(std::vector<int>* fds) override;
  IOResult WriteNoLock(size_t* platform_handles_written,
                       size_t* bytes_written) override;
  IOResult ScheduleWriteNoLock() override;
  void OnInit() override;
  void OnShutdownNoLock(scoped_ptr<ReadBuffer> read_buffer,
                        scoped_ptr<WriteBuffer> write_buffer) override;

  // |base::MessageLoopForIO::Watcher| implementation:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Implements most of |Read()| (except for a bit of clean-up):
  IOResult ReadImpl(size_t* bytes_read);

  // Watches for |fd_| to become writable. Must be called on the I/O thread.
  void WaitToWrite();

  ScopedPlatformHandle fd_;

  // The following members are only used on the I/O thread:
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> read_watcher_;
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> write_watcher_;

  bool pending_read_;

  std::deque<PlatformHandle> read_platform_handles_;

  // The following members are used on multiple threads and protected by
  // |write_lock()|:
  bool pending_write_;

  // This is used for posting tasks from write threads to the I/O thread. It
  // must only be accessed under |write_lock_|. The weak pointers it produces
  // are only used/invalidated on the I/O thread.
  base::WeakPtrFactory<RawChannelPosix> weak_ptr_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RawChannelPosix);
};

RawChannelPosix::RawChannelPosix(ScopedPlatformHandle handle)
    : fd_(std::move(handle)),
      pending_read_(false),
      pending_write_(false),
      weak_ptr_factory_(this) {
  DCHECK(fd_.is_valid());
}

RawChannelPosix::~RawChannelPosix() {
  DCHECK(!pending_read_);
  DCHECK(!pending_write_);

  // No need to take the |write_lock()| here -- if there are still weak pointers
  // outstanding, then we're hosed anyway (since we wouldn't be able to
  // invalidate them cleanly, since we might not be on the I/O thread).
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());

  // These must have been shut down/destroyed on the I/O thread.
  DCHECK(!read_watcher_);
  DCHECK(!write_watcher_);

  CloseAllPlatformHandles(&read_platform_handles_);
}

void RawChannelPosix::EnqueueMessageNoLock(
    scoped_ptr<MessageInTransit> message) {
  if (message->transport_data()) {
    PlatformHandleVector* const platform_handles =
        message->transport_data()->platform_handles();
    if (platform_handles &&
        platform_handles->size() > kPlatformChannelMaxNumHandles) {
      // We can't attach all the FDs to a single message, so we have to "split"
      // the message. Send as many control messages as needed first with FDs
      // attached (and no data).
      size_t i = 0;
      for (; platform_handles->size() - i > kPlatformChannelMaxNumHandles;
           i += kPlatformChannelMaxNumHandles) {
        scoped_ptr<MessageInTransit> fd_message(new MessageInTransit(
            MessageInTransit::Type::RAW_CHANNEL_POSIX_EXTRA_PLATFORM_HANDLES, 0,
            nullptr));
        ScopedPlatformHandleVectorPtr fds(
            new PlatformHandleVector(
                platform_handles->begin() + i,
                platform_handles->begin() + i + kPlatformChannelMaxNumHandles));
        fd_message->SetTransportData(make_scoped_ptr(new TransportData(
            std::move(fds), GetSerializedPlatformHandleSize())));
        RawChannel::EnqueueMessageNoLock(std::move(fd_message));
      }

      // Remove the handles that we "moved" into the other messages.
      platform_handles->erase(platform_handles->begin(),
                              platform_handles->begin() + i);
    }
  }

  RawChannel::EnqueueMessageNoLock(std::move(message));
}

bool RawChannelPosix::OnReadMessageForRawChannel(
    const MessageInTransit::View& message_view) {
  if (message_view.type() ==
      MessageInTransit::Type::RAW_CHANNEL_POSIX_EXTRA_PLATFORM_HANDLES) {
    // We don't need to do anything. |RawChannel| won't extract the platform
    // handles, and they'll be accumulated in |Read()|.
    return true;
  }

  return RawChannel::OnReadMessageForRawChannel(message_view);
}


ScopedPlatformHandle RawChannelPosix::ReleaseHandleNoLock(
    std::vector<char>* serialized_read_buffer,
    std::vector<char>* serialized_write_buffer,
    std::vector<int>* serialized_read_fds,
    std::vector<int>* serialized_write_fds,
    bool* write_error) {
  read_watcher_.reset();
  write_watcher_.reset();

  SerializeReadBuffer(0u, serialized_read_buffer);
  SerializeWriteBuffer(0u, 0u, serialized_write_buffer, serialized_write_fds);

  while (!read_platform_handles_.empty()) {
    serialized_read_fds->push_back(read_platform_handles_.front().handle);
    read_platform_handles_.pop_front();
  }

  return std::move(fd_);
}

void RawChannelPosix::SetSerializedFDs(
    std::vector<int>* serialized_read_fds,
    std::vector<int>* serialized_write_fds) {
  if (serialized_read_fds) {
    for(auto i: *serialized_read_fds)
      read_platform_handles_.push_back(PlatformHandle(i));
  }

  if (serialized_write_fds) {
    size_t i = 0;
    while (i < serialized_write_fds->size()) {
      size_t batch = std::min(kPlatformChannelMaxNumHandles,
                              serialized_write_fds->size() - i);
      scoped_ptr<MessageInTransit> fd_message(new MessageInTransit(
          MessageInTransit::Type::RAW_CHANNEL_POSIX_EXTRA_PLATFORM_HANDLES, 0,
          nullptr));
      ScopedPlatformHandleVectorPtr fds(
          new PlatformHandleVector(serialized_write_fds->begin() + i,
                                   serialized_write_fds->begin() + i + batch));
      fd_message->SetTransportData(make_scoped_ptr(new TransportData(
          std::move(fds), GetSerializedPlatformHandleSize())));
      RawChannel::EnqueueMessageNoLock(std::move(fd_message));
      i += batch;
    }
  }
}

bool RawChannelPosix::IsHandleValid() {
  return fd_.is_valid();
}

RawChannel::IOResult RawChannelPosix::Read(size_t* bytes_read) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  DCHECK(!pending_read_);

  IOResult rv = ReadImpl(bytes_read);
  if (rv != IO_SUCCEEDED && rv != IO_PENDING) {
    // Make sure that |OnFileCanReadWithoutBlocking()| won't be called again.
    read_watcher_.reset();
  }
  return rv;
}

RawChannel::IOResult RawChannelPosix::ScheduleRead() {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  DCHECK(!pending_read_);

  pending_read_ = true;

  return IO_PENDING;
}

ScopedPlatformHandleVectorPtr RawChannelPosix::GetReadPlatformHandles(
    size_t num_platform_handles,
    const void* /*platform_handle_table*/) {
  DCHECK_GT(num_platform_handles, 0u);

  if (read_platform_handles_.size() < num_platform_handles) {
    CloseAllPlatformHandles(&read_platform_handles_);
    read_platform_handles_.clear();
    return ScopedPlatformHandleVectorPtr();
  }

  ScopedPlatformHandleVectorPtr rv(
      new PlatformHandleVector(num_platform_handles));
  rv->assign(read_platform_handles_.begin(),
             read_platform_handles_.begin() + num_platform_handles);
  read_platform_handles_.erase(
      read_platform_handles_.begin(),
      read_platform_handles_.begin() + num_platform_handles);
  return rv;
}

size_t RawChannelPosix::SerializePlatformHandles(std::vector<int>* fds) {
  if (!write_buffer_no_lock()->HavePlatformHandlesToSend())
    return 0u;

  size_t num_platform_handles;
  PlatformHandle* platform_handles;
  void* serialization_data;  // Actually unused.
  write_buffer_no_lock()->GetPlatformHandlesToSend(
      &num_platform_handles, &platform_handles, &serialization_data);
  DCHECK_GT(num_platform_handles, 0u);
  DCHECK_LE(num_platform_handles, kPlatformChannelMaxNumHandles);
  for (size_t i = 0; i < num_platform_handles; ++i)
    fds->push_back(platform_handles[i].handle);
  return num_platform_handles;
}

RawChannel::IOResult RawChannelPosix::WriteNoLock(
    size_t* platform_handles_written,
    size_t* bytes_written) {
  write_lock().AssertAcquired();

  DCHECK(!pending_write_);

  size_t num_platform_handles = 0;
  ssize_t write_result;
  if (write_buffer_no_lock()->HavePlatformHandlesToSend()) {
    PlatformHandle* platform_handles;
    void* serialization_data;  // Actually unused.
    write_buffer_no_lock()->GetPlatformHandlesToSend(
        &num_platform_handles, &platform_handles, &serialization_data);
    DCHECK_GT(num_platform_handles, 0u);
    DCHECK_LE(num_platform_handles, kPlatformChannelMaxNumHandles);
    DCHECK(platform_handles);

    // TODO(vtl): Reduce code duplication. (This is duplicated from below.)
    std::vector<WriteBuffer::Buffer> buffers;
    write_buffer_no_lock()->GetBuffers(&buffers);
    DCHECK(!buffers.empty());
    const size_t kMaxBufferCount = 10;
    iovec iov[kMaxBufferCount];
    size_t buffer_count = std::min(buffers.size(), kMaxBufferCount);
    for (size_t i = 0; i < buffer_count; ++i) {
      iov[i].iov_base = const_cast<char*>(buffers[i].addr);
      iov[i].iov_len = buffers[i].size;
    }

    write_result = PlatformChannelSendmsgWithHandles(
        fd_.get(), iov, buffer_count, platform_handles, num_platform_handles);
    if (write_result >= 0) {
      for (size_t i = 0; i < num_platform_handles; i++)
        platform_handles[i].CloseIfNecessary();
    }
  } else {
    std::vector<WriteBuffer::Buffer> buffers;
    write_buffer_no_lock()->GetBuffers(&buffers);
    DCHECK(!buffers.empty());

    if (buffers.size() == 1) {
      write_result = PlatformChannelWrite(fd_.get(), buffers[0].addr,
                                          buffers[0].size);
    } else {
      const size_t kMaxBufferCount = 10;
      iovec iov[kMaxBufferCount];
      size_t buffer_count = std::min(buffers.size(), kMaxBufferCount);
      for (size_t i = 0; i < buffer_count; ++i) {
        iov[i].iov_base = const_cast<char*>(buffers[i].addr);
        iov[i].iov_len = buffers[i].size;
      }

      write_result = PlatformChannelWritev(fd_.get(), iov, buffer_count);
    }
  }

  if (write_result >= 0) {
    *platform_handles_written = num_platform_handles;
    *bytes_written = static_cast<size_t>(write_result);
    return IO_SUCCEEDED;
  }

  if (errno == EPIPE)
    return IO_FAILED_SHUTDOWN;

  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(WARNING) << "sendmsg/write/writev";
    return IO_FAILED_UNKNOWN;
  }

  return ScheduleWriteNoLock();
}

RawChannel::IOResult RawChannelPosix::ScheduleWriteNoLock() {
  write_lock().AssertAcquired();

  DCHECK(!pending_write_);

  // Set up to wait for the FD to become writable.
  // If we're not on the I/O thread, we have to post a task to do this.
  if (!internal::g_io_thread_task_runner->RunsTasksOnCurrentThread()) {
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&RawChannelPosix::WaitToWrite,
                   weak_ptr_factory_.GetWeakPtr()));
    pending_write_ = true;
    return IO_PENDING;
  }

  if (base::MessageLoopForIO::current()->WatchFileDescriptor(
          fd_.get().handle, false, base::MessageLoopForIO::WATCH_WRITE,
          write_watcher_.get(), this)) {
    pending_write_ = true;
    return IO_PENDING;
  }

  return IO_FAILED_UNKNOWN;
}

void RawChannelPosix::OnInit() {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  if (!fd_.is_valid()) {
    DVLOG(1) << "Note: RawChannelPOSIX " << this << " early exiting in OnInit "
             << "because there's no fd. This is valid if it's been sent.";
    return;
  }

  DCHECK(!read_watcher_);
  read_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher());
  DCHECK(!write_watcher_);
  write_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher());

  // I don't know how this can fail (unless |fd_| is bad, in which case it's a
  // bug in our code). I also don't know if |WatchFileDescriptor()| actually
  // fails cleanly.
  CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
      fd_.get().handle, true, base::MessageLoopForIO::WATCH_READ,
      read_watcher_.get(), this));
}

void RawChannelPosix::OnShutdownNoLock(
    scoped_ptr<ReadBuffer> /*read_buffer*/,
    scoped_ptr<WriteBuffer> /*write_buffer*/) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  write_lock().AssertAcquired();

  read_watcher_.reset();   // This will stop watching (if necessary).
  write_watcher_.reset();  // This will stop watching (if necessary).

  pending_read_ = false;
  pending_write_ = false;

  fd_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RawChannelPosix::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock locker(read_lock());
  if (!fd_.is_valid()) {
    pending_read_ = false;
    return;  // ReleaseHandle has been called.
  }

  if (!pending_read_) {
    NOTREACHED();
    return;
  }

  pending_read_ = false;
  size_t bytes_read = 0;
  IOResult io_result = Read(&bytes_read);
  if (io_result != IO_PENDING) {
    OnReadCompletedNoLock(io_result, bytes_read);
    // TODO(vtl): If we weren't destroyed, we'd like to do
    //
    //   DCHECK(!read_watcher_ || pending_read_);
    //
    // On failure, |read_watcher_| must have been reset; on success, we assume
    // that |OnReadCompleted()| always schedules another read. Otherwise, we
    // could end up spinning -- getting |OnFileCanReadWithoutBlocking()| again
    // and again but not doing any actual read.
    // TODO(yzshen): An alternative is to stop watching if RawChannel doesn't
    // schedule a new read. But that code won't be reached under the current
    // RawChannel implementation.
    return;  // |this| may have been destroyed in |OnReadCompleted()|.
  }

  DCHECK(pending_read_);
}

void RawChannelPosix::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock locker(read_lock());
  if (!fd_.is_valid()) {
    pending_write_ = false;
    return;  // ReleaseHandle has been called.
  }

  IOResult io_result;
  size_t platform_handles_written = 0;
  size_t bytes_written = 0;
  {
    base::AutoLock locker(write_lock());

    DCHECK(pending_write_);

    pending_write_ = false;
    io_result = WriteNoLock(&platform_handles_written, &bytes_written);
  }

  if (io_result != IO_PENDING) {
    base::AutoLock locker(write_lock());
    OnWriteCompletedNoLock(io_result, platform_handles_written, bytes_written);
    return;
  }
}

RawChannel::IOResult RawChannelPosix::ReadImpl(size_t* bytes_read) {
  char* buffer = nullptr;
  size_t bytes_to_read = 0;
  read_buffer()->GetBuffer(&buffer, &bytes_to_read);

  size_t old_num_platform_handles = read_platform_handles_.size();
  ssize_t read_result = PlatformChannelRecvmsg(
      fd_.get(), buffer, bytes_to_read, &read_platform_handles_);
  if (read_platform_handles_.size() > old_num_platform_handles) {
    DCHECK_LE(read_platform_handles_.size() - old_num_platform_handles,
              kPlatformChannelMaxNumHandles);

    // We should never accumulate more than |TransportData::kMaxPlatformHandles
    // + kPlatformChannelMaxNumHandles| handles. (The latter part is
    // possible because we could have accumulated all the handles for a message,
    // then received the message data plus the first set of handles for the next
    // message in the subsequent |recvmsg()|.)
    if (read_platform_handles_.size() >
        (TransportData::GetMaxPlatformHandles() +
         kPlatformChannelMaxNumHandles)) {
      LOG(ERROR) << "Received too many platform handles";
      CloseAllPlatformHandles(&read_platform_handles_);
      read_platform_handles_.clear();
      return IO_FAILED_UNKNOWN;
    }
  }

  if (read_result > 0) {
    *bytes_read = static_cast<size_t>(read_result);
    return IO_SUCCEEDED;
  }

  // |read_result == 0| means "end of file".
  if (read_result == 0)
    return IO_FAILED_SHUTDOWN;

  if (errno == EAGAIN || errno == EWOULDBLOCK)
    return ScheduleRead();

  if (errno == ECONNRESET)
    return IO_FAILED_BROKEN;

  PLOG(WARNING) << "recvmsg";
  return IO_FAILED_UNKNOWN;
}

void RawChannelPosix::WaitToWrite() {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());

  DCHECK(write_watcher_);

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          fd_.get().handle, false, base::MessageLoopForIO::WATCH_WRITE,
          write_watcher_.get(), this)) {
    base::AutoLock locker(write_lock());
    DCHECK(pending_write_);
    pending_write_ = false;
    OnWriteCompletedNoLock(IO_FAILED_UNKNOWN, 0, 0);
    return;
  }
}

}  // namespace

// -----------------------------------------------------------------------------

// Static factory method declared in raw_channel.h.
// static
RawChannel* RawChannel::Create(ScopedPlatformHandle handle) {
  return new RawChannelPosix(std::move(handle));
}

size_t RawChannel::GetSerializedPlatformHandleSize() {
  // We don't actually need any space on POSIX (since we just send FDs).
  return 0;
}

bool RawChannel::IsOtherEndOf(RawChannel* other) {
#if defined(OFFICIAL_BUILD)
  return false;
#else
  // We don't check the return code of getsockopt because this is only available
  // on Linux after 3.4. This is a developer error, so we just have to catch it
  // on platforms that developers use.
  // Note that since we're storing a 32 bit integer, we can get collisions so we
  // will only use it in non-official builds.
  DCHECK_NE(other, this);
  PlatformHandle this_handle = static_cast<RawChannelPosix*>(this)->GetFD();
  PlatformHandle other_handle = static_cast<RawChannelPosix*>(other)->GetFD();

  int id1 = 0;
  int id2 = 1;
  socklen_t peek_off_size = sizeof(id1);
  getsockopt(this_handle.handle, SOL_SOCKET, SO_PEEK_OFF, &id1, &peek_off_size);
  getsockopt(other_handle.handle, SOL_SOCKET, SO_PEEK_OFF, &id2,
             &peek_off_size);
  return id1 == id2;
#endif
}

}  // namespace edk
}  // namespace mojo
