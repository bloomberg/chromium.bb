// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/raw_channel.h"

#include <windows.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_version.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/public/cpp/system/macros.h"

#define STATUS_CANCELLED 0xC0000120
#define STATUS_PIPE_BROKEN 0xC000014B

// We can't use IO completion ports if we send a message pipe. The reason is
// that the only way to stop an existing IOCP is by closing the pipe handle.
// See https://msdn.microsoft.com/en-us/library/windows/hardware/ff545834(v=vs.85).aspx
bool g_use_iocp = false;

// Manual reset per
// Doc for overlapped I/O says use manual per
//   https://msdn.microsoft.com/en-us/library/windows/desktop/ms684342(v=vs.85).aspx
// However using an auto-reset event makes the perf test 5x faster and also
// works since we don't wait on the event elsewhere or call GetOverlappedResult
// before it fires.
bool g_use_autoreset_event = true;

namespace mojo {
namespace edk {

namespace {

struct MOJO_ALIGNAS(8) SerializedHandle {
  DWORD handle_pid;
  HANDLE handle;
};

class VistaOrHigherFunctions {
 public:
  VistaOrHigherFunctions()
      : is_vista_or_higher_(
          base::win::GetVersion() >= base::win::VERSION_VISTA),
        set_file_completion_notification_modes_(nullptr),
        cancel_io_ex_(nullptr) {
    if (!is_vista_or_higher_)
      return;

    HMODULE module = GetModuleHandleW(L"kernel32.dll");
    set_file_completion_notification_modes_ =
        reinterpret_cast<SetFileCompletionNotificationModesFunc>(
            GetProcAddress(module, "SetFileCompletionNotificationModes"));
    DCHECK(set_file_completion_notification_modes_);

    cancel_io_ex_ =
        reinterpret_cast<CancelIoExFunc>(GetProcAddress(module, "CancelIoEx"));
    DCHECK(cancel_io_ex_);
  }

  bool is_vista_or_higher() const { return is_vista_or_higher_; }

  BOOL SetFileCompletionNotificationModes(HANDLE handle, UCHAR flags) {
    return set_file_completion_notification_modes_(handle, flags);
  }

  BOOL CancelIoEx(HANDLE handle, LPOVERLAPPED overlapped) {
    return cancel_io_ex_(handle, overlapped);
  }

 private:
  using SetFileCompletionNotificationModesFunc = BOOL(WINAPI*)(HANDLE, UCHAR);
  using CancelIoExFunc = BOOL(WINAPI*)(HANDLE, LPOVERLAPPED);

  bool is_vista_or_higher_;
  SetFileCompletionNotificationModesFunc
      set_file_completion_notification_modes_;
  CancelIoExFunc cancel_io_ex_;
};

base::LazyInstance<VistaOrHigherFunctions> g_vista_or_higher_functions =
    LAZY_INSTANCE_INITIALIZER;

class RawChannelWin final : public RawChannel {
 public:
  RawChannelWin(ScopedPlatformHandle handle)
      : handle_(handle.Pass()),
        io_handler_(nullptr),
        skip_completion_port_on_success_(
            g_use_iocp &&
            g_vista_or_higher_functions.Get().is_vista_or_higher()) {
    DCHECK(handle_.is_valid());
  }
  ~RawChannelWin() override {
    DCHECK(!io_handler_);
  }

 private:
  // RawChannelIOHandler receives OS notifications for I/O completion. It must
  // be created on the I/O thread.
  //
  // It manages its own destruction. Destruction happens on the I/O thread when
  // all the following conditions are satisfied:
  //   - |DetachFromOwnerNoLock()| has been called;
  //   - there is no pending read;
  //   - there is no pending write.
  class RawChannelIOHandler : public base::MessageLoopForIO::IOHandler,
    public base::win::ObjectWatcher::Delegate {
   public:
    RawChannelIOHandler(RawChannelWin* owner,
                        ScopedPlatformHandle handle)
        : handle_(handle.Pass()),
          owner_(owner),
          suppress_self_destruct_(false),
          pending_read_(false),
          pending_write_(false),
          platform_handles_written_(0) {
      memset(&read_context_.overlapped, 0, sizeof(read_context_.overlapped));
      memset(&write_context_.overlapped, 0, sizeof(write_context_.overlapped));
      if (g_use_iocp) {
        owner_->message_loop_for_io()->RegisterIOHandler(
            handle_.get().handle, this);
        read_context_.handler = this;
        write_context_.handler = this;
      } else {
        read_event = CreateEvent(
            NULL, g_use_autoreset_event ? FALSE : TRUE, FALSE, NULL);
        write_event = CreateEvent(
            NULL, g_use_autoreset_event ? FALSE : TRUE, FALSE, NULL);
        read_context_.overlapped.hEvent = read_event;
        write_context_.overlapped.hEvent = write_event;


        if (g_use_autoreset_event) {
          read_watcher_.StartWatchingMultipleTimes(read_event, this);
          write_watcher_.StartWatchingMultipleTimes(write_event, this);
        }
      }
    }

    ~RawChannelIOHandler() override {
      DCHECK(ShouldSelfDestruct());
    }

    HANDLE handle() const { return handle_.get().handle; }

    // The following methods are only called by the owner on the I/O thread.
    bool pending_read() const {
      DCHECK(owner_);
      DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
      return pending_read_;
    }

    base::MessageLoopForIO::IOContext* read_context() {
      DCHECK(owner_);
      DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
      return &read_context_;
    }

    // Instructs the object to wait for an |OnIOCompleted()| notification.
    void OnPendingReadStarted() {
      DCHECK(owner_);
      DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
      DCHECK(!pending_read_);
      pending_read_ = true;
    }

    // The following methods are only called by the owner under
    // |owner_->write_lock()|.
    bool pending_write_no_lock() const {
      DCHECK(owner_);
      owner_->write_lock().AssertAcquired();
      return pending_write_;
    }

    base::MessageLoopForIO::IOContext* write_context_no_lock() {
      DCHECK(owner_);
      owner_->write_lock().AssertAcquired();
      return &write_context_;
    }
    // Instructs the object to wait for an |OnIOCompleted()| notification.
    void OnPendingWriteStartedNoLock(size_t platform_handles_written) {
      DCHECK(owner_);
      owner_->write_lock().AssertAcquired();
      DCHECK(!pending_write_);
      pending_write_ = true;
      platform_handles_written_ = platform_handles_written;
    }

    // |base::MessageLoopForIO::IOHandler| implementation:
    // Must be called on the I/O thread. It could be called before or after
    // detached from the owner.
    void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                       DWORD bytes_transferred,
                       DWORD error) override {
      DCHECK(!owner_ ||
             base::MessageLoop::current() == owner_->message_loop_for_io());

      // Suppress self-destruction inside |OnReadCompleted()|, etc. (in case
      // they result in a call to |Shutdown()|).
      bool old_suppress_self_destruct = suppress_self_destruct_;
      suppress_self_destruct_ = true;

      if (context == &read_context_)
        OnReadCompleted(bytes_transferred, error);
      else if (context == &write_context_)
        OnWriteCompleted(bytes_transferred, error);
      else
        NOTREACHED();

      // Maybe allow self-destruction again.
      suppress_self_destruct_ = old_suppress_self_destruct;

      if (ShouldSelfDestruct())
        delete this;
    }

    // Must be called on the I/O thread under |owner_->write_lock()|.
    // After this call, the owner must not make any further calls on this
    // object, and therefore the object is used on the I/O thread exclusively
    // (if it stays alive).
    void DetachFromOwnerNoLock(scoped_ptr<ReadBuffer> read_buffer,
                               scoped_ptr<WriteBuffer> write_buffer) {
      DCHECK(owner_);
      DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
      //owner_->write_lock().AssertAcquired();

      // If read/write is pending, we have to retain the corresponding buffer.
      if (pending_read_)
        preserved_read_buffer_after_detach_ = read_buffer.Pass();
      if (pending_write_)
        preserved_write_buffer_after_detach_ = write_buffer.Pass();

      owner_ = nullptr;
      if (ShouldSelfDestruct())
        delete this;
    }

    ScopedPlatformHandle ReleaseHandle(std::vector<char>* read_buffer) {
      // TODO(jam): handle XP
      CancelIoEx(handle(), NULL);
      // NOTE: The above call will cancel pending IO calls.
      size_t read_buffer_byte_size = owner_->read_buffer()->num_valid_bytes();

      if (pending_read_) {
        DWORD bytes_read_dword = 0;

        DWORD old_bytes = read_context_.overlapped.InternalHigh;

        // Since we cancelled pending IO calls above, we need to know if the
        // read did succeed (i.e. it completed and there's a pending task posted
        // to alert us) or if it was cancelled. This important because if the
        // read completed, we don't want to serialize those bytes again.
        //TODO(jam): for XP, can return TRUE here to wait. also below.
        BOOL rv = GetOverlappedResult(
            handle(), &read_context_.overlapped, &bytes_read_dword, FALSE);
        DCHECK_EQ(old_bytes, bytes_read_dword);
        if (rv) {
          if (read_context_.overlapped.Internal != STATUS_CANCELLED) {
            read_buffer_byte_size += read_context_.overlapped.InternalHigh;
          }
        }
        pending_read_ = false;
      }

      RawChannel::WriteBuffer* write_buffer = owner_->write_buffer_no_lock();

      if (pending_write_) {
        DWORD bytes_written_dword = 0;
        DWORD old_bytes = write_context_.overlapped.InternalHigh;

        // See comment above.
        BOOL rv = GetOverlappedResult(
            handle(), &write_context_.overlapped, &bytes_written_dword, FALSE);

        if (old_bytes != bytes_written_dword) {
          NOTREACHED() << "TODO(jam): this shouldn't be reached";
        }

        if (rv) {
          if (write_context_.overlapped.Internal != STATUS_CANCELLED) {
            CHECK(write_buffer->queue_size() != 0);

            // TODO(jam)
            DCHECK(!write_buffer->HavePlatformHandlesToSend());

            write_buffer->data_offset_ += bytes_written_dword;

            // TODO(jam): copied from OnWriteCompletedNoLock
            MessageInTransit* message =
                write_buffer->message_queue()->PeekMessage();
            if (write_buffer->data_offset_ >= message->total_size()) {
              // Complete write.
              CHECK_EQ(write_buffer->data_offset_, message->total_size());
              write_buffer->message_queue_.DiscardMessage();
              write_buffer->platform_handles_offset_ = 0;
              write_buffer->data_offset_ = 0;
            }


            //TODO(jam): handle more write msgs
            DCHECK(write_buffer->message_queue_.IsEmpty());
          }
        }
        pending_write_ = false;
      }

      if (read_buffer_byte_size) {
        read_buffer->resize(read_buffer_byte_size);
        memcpy(&(*read_buffer)[0], owner_->read_buffer()->buffer(),
               read_buffer_byte_size);
        owner_->read_buffer()->Reset();
      }

      return ScopedPlatformHandle(handle_.release());
    }

    void OnObjectSignaled(HANDLE object) override {
      // Since this is called on the IO thread, no locks needed for owner_.
      bool handle_is_valid = false;
      if (owner_)
        owner_->read_lock().Acquire();
      handle_is_valid = handle_.is_valid();
      if (owner_)
        owner_->read_lock().Release();
      if (!handle_is_valid) {
        if (object == read_event)
          pending_read_ = false;
        else
          pending_write_ = false;
        if (ShouldSelfDestruct())
          delete this;
        return;
      }

      if (object == read_event) {
        OnIOCompleted(&read_context_, read_context_.overlapped.InternalHigh,
                      read_context_.overlapped.Internal);

      } else {
        CHECK(object == write_event);
        OnIOCompleted(&write_context_, write_context_.overlapped.InternalHigh,
                      write_context_.overlapped.Internal);
      }
    }
    HANDLE read_event, write_event;
    base::win::ObjectWatcher read_watcher_, write_watcher_;

   private:
    // Returns true if |owner_| has been reset and there is not pending read or
    // write.
    // Must be called on the I/O thread.
    bool ShouldSelfDestruct() const {
      if (owner_ || suppress_self_destruct_)
        return false;

      // Note: Detached, hence no lock needed for |pending_write_|.
      return !pending_read_ && !pending_write_;
    }

    // Must be called on the I/O thread. It may be called before or after
    // detaching from the owner.
    void OnReadCompleted(DWORD bytes_read, DWORD error) {
      DCHECK(!owner_ ||
             base::MessageLoop::current() == owner_->message_loop_for_io());
      DCHECK(suppress_self_destruct_);

      if (g_use_autoreset_event && !pending_read_)
        return;

      CHECK(pending_read_);
      pending_read_ = false;
      if (!owner_)
        return;

      // Note: |OnReadCompleted()| may detach us from |owner_|.
      if (error == ERROR_SUCCESS) {
        DCHECK_GT(bytes_read, 0u);
        owner_->OnReadCompleted(IO_SUCCEEDED, bytes_read);
      } else if (error == ERROR_BROKEN_PIPE  ||
                 (g_use_autoreset_event && error == STATUS_PIPE_BROKEN)) {
        DCHECK_EQ(bytes_read, 0u);
        owner_->OnReadCompleted(IO_FAILED_SHUTDOWN, 0);
      } else if (error == ERROR_NO_MORE_ITEMS && g_use_autoreset_event) {
        return owner_->OnReadCompleted(IO_SUCCEEDED, bytes_read);
      } else {
        DCHECK_EQ(bytes_read, 0u);
        LOG(WARNING) << "ReadFile: " << logging::SystemErrorCodeToString(error);
        owner_->OnReadCompleted(IO_FAILED_UNKNOWN, 0);
      }
    }

    // Must be called on the I/O thread. It may be called before or after
    // detaching from the owner.
    void OnWriteCompleted(DWORD bytes_written, DWORD error) {
      DCHECK(!owner_ ||
             base::MessageLoop::current() == owner_->message_loop_for_io());
      DCHECK(suppress_self_destruct_);

      if (!owner_) {
        // No lock needed.
        CHECK(pending_write_);
        pending_write_ = false;
        return;
      }

      {
        base::AutoLock locker(owner_->write_lock());
        if (g_use_autoreset_event && !pending_write_)
          return;

        CHECK(pending_write_);
        pending_write_ = false;
      }

      // Note: |OnWriteCompleted()| may detach us from |owner_|.
      if (error == ERROR_SUCCESS) {
        // Reset |platform_handles_written_| before calling |OnWriteCompleted()|
        // since that function may call back to this class and set it again.
        size_t local_platform_handles_written_ = platform_handles_written_;
        platform_handles_written_ = 0;
        owner_->OnWriteCompleted(IO_SUCCEEDED, local_platform_handles_written_,
                                 bytes_written);
      } else if (error == ERROR_BROKEN_PIPE  ||
                 (g_use_autoreset_event && error == STATUS_PIPE_BROKEN)) {
        owner_->OnWriteCompleted(IO_FAILED_SHUTDOWN, 0, 0);
      } else if (error == ERROR_NO_MORE_ITEMS && g_use_autoreset_event) {
         size_t local_platform_handles_written_ = platform_handles_written_;
        platform_handles_written_ = 0;
        owner_->OnWriteCompleted(IO_SUCCEEDED, local_platform_handles_written_,
                                 bytes_written);
      } else {
        LOG(WARNING) << "WriteFile: "
                     << logging::SystemErrorCodeToString(error);
        owner_->OnWriteCompleted(IO_FAILED_UNKNOWN, 0, 0);
      }
    }

    ScopedPlatformHandle handle_;

    // |owner_| is reset on the I/O thread under |owner_->write_lock()|.
    // Therefore, it may be used on any thread under lock; or on the I/O thread
    // without locking.
    RawChannelWin* owner_;

    // The following members must be used on the I/O thread.
    scoped_ptr<ReadBuffer> preserved_read_buffer_after_detach_;
    scoped_ptr<WriteBuffer> preserved_write_buffer_after_detach_;
    bool suppress_self_destruct_;

    bool pending_read_;
    base::MessageLoopForIO::IOContext read_context_;

    // The following members must be used under |owner_->write_lock()| while the
    // object is still attached to the owner, and only on the I/O thread
    // afterwards.
    bool pending_write_;
    size_t platform_handles_written_;
    base::MessageLoopForIO::IOContext write_context_;

    MOJO_DISALLOW_COPY_AND_ASSIGN(RawChannelIOHandler);
  };

  ScopedPlatformHandle ReleaseHandleNoLock(
      std::vector<char>* read_buffer_out) override {
    std::vector<WriteBuffer::Buffer> buffers;
    write_buffer_no_lock()->GetBuffers(&buffers);
    if (!buffers.empty()) {
      // TODO(jam): copy code in OnShutdownNoLock
      NOTREACHED() << "releasing handle with pending write buffer";
    }


    if (handle_.is_valid()) {
      // SetInitialBuffer could have been called on main thread before OnInit
      // is called on Io thread. and in meantime releasehandle called.
      //DCHECK(read_buffer()->num_valid_bytes() == 0);
      if (read_buffer()->num_valid_bytes()) {
        read_buffer_out->resize(read_buffer()->num_valid_bytes());
        memcpy(&(*read_buffer_out)[0], read_buffer()->buffer(),
               read_buffer()->num_valid_bytes());
        read_buffer()->Reset();
      }
      DCHECK(write_buffer_no_lock()->queue_size() == 0);
      return ScopedPlatformHandle(PlatformHandle(handle_.release().handle));
    }

    return io_handler_->ReleaseHandle(read_buffer_out);
  }
  PlatformHandle HandleForDebuggingNoLock() override {
    if (handle_.is_valid())
      return handle_.get();

    if (!io_handler_)
      return PlatformHandle();

    return PlatformHandle(io_handler_->handle());
  }

  IOResult Read(size_t* bytes_read) override  {
    DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

    char* buffer = nullptr;
    size_t bytes_to_read = 0;
    read_buffer()->GetBuffer(&buffer, &bytes_to_read);

    DCHECK(io_handler_);
    DCHECK(!io_handler_->pending_read());
    BOOL result = ReadFile(
        io_handler_->handle(), buffer, static_cast<DWORD>(bytes_to_read),
        nullptr, &io_handler_->read_context()->overlapped);
    if (!result) {
      DWORD error = GetLastError();
      if (error == ERROR_BROKEN_PIPE)
        return IO_FAILED_SHUTDOWN;
      if (error != ERROR_IO_PENDING) {
        LOG(WARNING) << "ReadFile: " << logging::SystemErrorCodeToString(error);
        return IO_FAILED_UNKNOWN;
      }
    }

    if (result && skip_completion_port_on_success_) {
      DWORD bytes_read_dword = 0;
      BOOL get_size_result = GetOverlappedResult(
          io_handler_->handle(), &io_handler_->read_context()->overlapped,
          &bytes_read_dword, FALSE);
      DPCHECK(get_size_result);
      *bytes_read = bytes_read_dword;
      return IO_SUCCEEDED;
    }

    if (!g_use_autoreset_event) {
      if (!g_use_iocp) {
        io_handler_->read_watcher_.StartWatchingOnce(
            io_handler_->read_event, io_handler_);
      }
    }
    // If the read is pending or the read has succeeded but we don't skip
    // completion port on success, instruct |io_handler_| to wait for the
    // completion packet.
    //
    // TODO(yzshen): It seems there isn't document saying that all error cases
    // (other than ERROR_IO_PENDING) are guaranteed to *not* queue a completion
    // packet. If we do get one for errors,
    // |RawChannelIOHandler::OnIOCompleted()| will crash so we will learn about
    // it.

    io_handler_->OnPendingReadStarted();
    return IO_PENDING;
  }

  IOResult ScheduleRead() override {
    if (!io_handler_)
      return IO_PENDING;  // OnInit could have earlied out.

    DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
    DCHECK(io_handler_);
    DCHECK(!io_handler_->pending_read());

    size_t bytes_read = 0;
    IOResult io_result = Read(&bytes_read);
    if (io_result == IO_SUCCEEDED) {
      DCHECK(skip_completion_port_on_success_);

      // We have finished reading successfully. Queue a notification manually.
      io_handler_->OnPendingReadStarted();
      // |io_handler_| won't go away before the task is run, so it is safe to
      // use |base::Unretained()|.
      message_loop_for_io()->PostTask(
          FROM_HERE, base::Bind(&RawChannelIOHandler::OnIOCompleted,
                                base::Unretained(io_handler_),
                                base::Unretained(io_handler_->read_context()),
                                static_cast<DWORD>(bytes_read), ERROR_SUCCESS));
      return IO_PENDING;
    }

    return io_result;
  }
  ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_platform_handles,
      const void* platform_handle_table) override {
    // TODO(jam): this code will have to be updated once it's used in a sandbox
    // and the receiving process doesn't have duplicate permission for the
    // receiver. Once there's a broker and we have a connection to it (possibly
    // through ConnectionManager), then we can make a sync IPC to it here to get
    // a token for this handle, and it will duplicate the handle to is process.
    // Then we pass the token to the receiver, which will then make a sync call
    // to the broker to get a duplicated handle. This will also allow us to
    // avoid leaks of the handle if the receiver dies, since the broker can
    // notice that.
    DCHECK_GT(num_platform_handles, 0u);
    ScopedPlatformHandleVectorPtr rv(new PlatformHandleVector());

    const SerializedHandle* serialization_data =
        static_cast<const SerializedHandle*>(platform_handle_table);
    for (size_t i = 0; i < num_platform_handles; i++) {
      DWORD pid = serialization_data->handle_pid;
      HANDLE source_handle = serialization_data->handle;
      serialization_data ++;
      base::Process sender =
          base::Process::OpenWithAccess(pid, PROCESS_DUP_HANDLE);
      DCHECK(sender.IsValid());
      HANDLE target_handle = NULL;
      BOOL dup_result = DuplicateHandle(
          sender.Handle(), source_handle,
          base::GetCurrentProcessHandle(), &target_handle, 0,
          FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
      DCHECK(dup_result);
      rv->push_back(PlatformHandle(target_handle));
    }
    return rv.Pass();
  }

  IOResult WriteNoLock(size_t* platform_handles_written,
                       size_t* bytes_written) override  {
    write_lock().AssertAcquired();

    DCHECK(io_handler_);
    DCHECK(!io_handler_->pending_write_no_lock());

    size_t num_platform_handles = 0;
    if (write_buffer_no_lock()->HavePlatformHandlesToSend()) {
      // Since we're not sure which process might ultimately deserialize this
      // message, we can't duplicate the handle now. Instead, write the process
      // ID and handle now and let the receiver duplicate it.
      PlatformHandle* platform_handles;
      void* serialization_data_temp;
      write_buffer_no_lock()->GetPlatformHandlesToSend(
          &num_platform_handles, &platform_handles, &serialization_data_temp);
      SerializedHandle* serialization_data =
          static_cast<SerializedHandle*>(serialization_data_temp);
      DCHECK_GT(num_platform_handles, 0u);
      DCHECK(platform_handles);

      DWORD current_process_id = base::GetCurrentProcId();
      for (size_t i = 0; i < num_platform_handles; i++) {
        serialization_data->handle_pid = current_process_id;
        serialization_data->handle = platform_handles[i].handle;
        serialization_data++;
        platform_handles[i] = PlatformHandle();
      }
    }

    std::vector<WriteBuffer::Buffer> buffers;
    write_buffer_no_lock()->GetBuffers(&buffers);
    DCHECK(!buffers.empty());

    // TODO(yzshen): Handle multi-segment writes more efficiently.
    DWORD bytes_written_dword = 0;


    /* jam: comment below is commented out because with the latest code I don't
    see this. Note that this code is incorrect for two reasons:
    1) the buffer would need to be saved until the write is finished
    2) this still doesn't fix the problem of there being more data or messages
       to be sent. the proper fix is to serialize pending messages to shared
       memory.

    // TODO(jam): right now we get in bad situation where we might first write
    // the main buffer and then the MP gets sent before we write the transport
    // buffer. We can fix this by sending information about partially written
    // messages, or by teaching transport buffer how to grow the main buffer and
    // write its data there.
    // Until that's done, for now make another copy.

    size_t total_size = buffers[0].size;
    if (buffers.size() > 1)
      total_size+=buffers[1].size;
    char* buf = new char[total_size];
    memcpy(buf, buffers[0].addr, buffers[0].size);
    if (buffers.size() > 1)
      memcpy(buf + buffers[0].size, buffers[1].addr, buffers[1].size);

    BOOL result = WriteFile(
        io_handler_->handle(), buf,
        static_cast<DWORD>(total_size),
        &bytes_written_dword,
        &io_handler_->write_context_no_lock()->overlapped);
    delete [] buf;

    if (!result) {
      DWORD error = GetLastError();
      if (error == ERROR_BROKEN_PIPE)
        return IO_FAILED_SHUTDOWN;
      if (error != ERROR_IO_PENDING) {
        LOG(WARNING) << "WriteFile: "
                     << logging::SystemErrorCodeToString(error);
        return IO_FAILED_UNKNOWN;
      }
    }
    */

    BOOL result =
        WriteFile(io_handler_->handle(), buffers[0].addr,
                  static_cast<DWORD>(buffers[0].size), &bytes_written_dword,
                  &io_handler_->write_context_no_lock()->overlapped);
    if (!result) {
      DWORD error = GetLastError();
      if (error == ERROR_BROKEN_PIPE)
        return IO_FAILED_SHUTDOWN;
      if (error != ERROR_IO_PENDING) {
        LOG(WARNING) << "WriteFile: " << logging::SystemErrorCodeToString(error);
        return IO_FAILED_UNKNOWN;
      }
    }

    if (result && skip_completion_port_on_success_) {
      *platform_handles_written = num_platform_handles;
      *bytes_written = bytes_written_dword;
      return IO_SUCCEEDED;
    }

    if (!g_use_autoreset_event) {
      if (!g_use_iocp) {
        io_handler_->write_watcher_.StartWatchingOnce(
            io_handler_->write_event, io_handler_);
      }
    }
    // If the write is pending or the write has succeeded but we don't skip
    // completion port on success, instruct |io_handler_| to wait for the
    // completion packet.
    //
    // TODO(yzshen): it seems there isn't document saying that all error cases
    // (other than ERROR_IO_PENDING) are guaranteed to *not* queue a completion
    // packet. If we do get one for errors,
    // |RawChannelIOHandler::OnIOCompleted()| will crash so we will learn about
    // it.

    io_handler_->OnPendingWriteStartedNoLock(num_platform_handles);
    return IO_PENDING;
  }

  IOResult ScheduleWriteNoLock() override {
    write_lock().AssertAcquired();

    DCHECK(io_handler_);
    DCHECK(!io_handler_->pending_write_no_lock());

    size_t platform_handles_written = 0;
    size_t bytes_written = 0;
    IOResult io_result = WriteNoLock(&platform_handles_written, &bytes_written);
    if (io_result == IO_SUCCEEDED) {
      DCHECK(skip_completion_port_on_success_);

      // We have finished writing successfully. Queue a notification manually.
      io_handler_->OnPendingWriteStartedNoLock(platform_handles_written);
      // |io_handler_| won't go away before that task is run, so it is safe to
      // use |base::Unretained()|.
      message_loop_for_io()->PostTask(
          FROM_HERE,
          base::Bind(&RawChannelIOHandler::OnIOCompleted,
                     base::Unretained(io_handler_),
                     base::Unretained(io_handler_->write_context_no_lock()),
                     static_cast<DWORD>(bytes_written), ERROR_SUCCESS));
      return IO_PENDING;
    }

    return io_result;
  }

  void OnInit() override {
    DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

    if (!handle_.is_valid()) {
      LOG(ERROR) << "Note: RawChannelWin " << this
                 << " early exiting in OnInit because no handle";
      return;
    }

    DCHECK(handle_.is_valid());
    if (skip_completion_port_on_success_) {
      // I don't know how this can fail (unless |handle_| is bad, in which case
      // it's a bug in our code).
      CHECK(g_vista_or_higher_functions.Get().
          SetFileCompletionNotificationModes(
              handle_.get().handle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS));
    }

    DCHECK(!io_handler_);
    io_handler_ = new RawChannelIOHandler(this, handle_.Pass());
  }

  void OnShutdownNoLock(scoped_ptr<ReadBuffer> read_buffer,
                        scoped_ptr<WriteBuffer> write_buffer) override {
    // happens on shutdown if didn't call init when doing createduplicate
    if (message_loop_for_io()) {
      DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
    }

    if (!io_handler_) {
      // This is hit when creating a duplicate dispatcher since we don't call
      // Init on it.
      DCHECK_EQ(read_buffer->num_valid_bytes(), 0U);
      DCHECK_EQ(write_buffer->queue_size(), 0U);
      return;
    }

    if (io_handler_->pending_read() || io_handler_->pending_write_no_lock()) {
      // |io_handler_| will be alive until pending read/write (if any)
      // completes. Call |CancelIoEx()| or |CancelIo()| so that resources can be
      // freed up as soon as possible.
      // Note: |CancelIo()| only cancels read/write requests started from this
      // thread.
      if (g_vista_or_higher_functions.Get().is_vista_or_higher()) {
        g_vista_or_higher_functions.Get().CancelIoEx(io_handler_->handle(),
                                                     nullptr);
      } else {
        CancelIo(io_handler_->handle());
      }
    }

    io_handler_->DetachFromOwnerNoLock(read_buffer.Pass(), write_buffer.Pass());
    io_handler_ = nullptr;
  }

  // Passed to |io_handler_| during initialization.
  ScopedPlatformHandle handle_;

  RawChannelIOHandler* io_handler_;

  const bool skip_completion_port_on_success_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RawChannelWin);
};


}  // namespace

// -----------------------------------------------------------------------------

RawChannel* RawChannel::Create(ScopedPlatformHandle handle) {
  return new RawChannelWin(handle.Pass());
}

size_t RawChannel::GetSerializedPlatformHandleSize() {
  return sizeof(SerializedHandle);
}

bool RawChannel::IsOtherEndOf(RawChannel* other) {
  PlatformHandle this_handle = HandleForDebuggingNoLock();
  PlatformHandle other_handle = other->HandleForDebuggingNoLock();

  // TODO: XP: see http://stackoverflow.com/questions/65170/how-to-get-name-associated-with-open-handle/5286888#5286888
  WCHAR data1[_MAX_PATH + sizeof(FILE_NAME_INFO)];
  WCHAR data2[_MAX_PATH + sizeof(FILE_NAME_INFO)];
  FILE_NAME_INFO* fileinfo1 = reinterpret_cast<FILE_NAME_INFO *>(data1);
  FILE_NAME_INFO* fileinfo2 = reinterpret_cast<FILE_NAME_INFO *>(data2);
  CHECK(GetFileInformationByHandleEx(
      this_handle.handle, FileNameInfo, fileinfo1, arraysize(data1)));
  CHECK(GetFileInformationByHandleEx(
      other_handle.handle, FileNameInfo, fileinfo2, arraysize(data2)));
  std::wstring filepath1(fileinfo1->FileName, fileinfo1->FileNameLength / 2);
  std::wstring filepath2(fileinfo2->FileName, fileinfo2->FileNameLength / 2);
  return filepath1 == filepath2;
}

}  // namespace edk
}  // namespace mojo
