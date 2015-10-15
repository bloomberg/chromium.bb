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
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/edk/system/transport_data.h"
#include "mojo/public/cpp/system/macros.h"

#define STATUS_CANCELLED 0xC0000120
#define STATUS_PIPE_BROKEN 0xC000014B

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
        cancel_io_ex_(nullptr),
        get_file_information_by_handle_ex_(nullptr) {
    if (!is_vista_or_higher_)
      return;

    HMODULE module = GetModuleHandleW(L"kernel32.dll");
    cancel_io_ex_ =
        reinterpret_cast<CancelIoExFunc>(GetProcAddress(module, "CancelIoEx"));
    DCHECK(cancel_io_ex_);

    get_file_information_by_handle_ex_ =
        reinterpret_cast<GetFileInformationByHandleExFunc>(
            GetProcAddress(module, "GetFileInformationByHandleEx"));
    DCHECK(get_file_information_by_handle_ex_);
  }

  bool is_vista_or_higher() const { return is_vista_or_higher_; }

  BOOL CancelIoEx(HANDLE handle, LPOVERLAPPED overlapped) {
    return cancel_io_ex_(handle, overlapped);
  }

  BOOL GetFileInformationByHandleEx(HANDLE handle,
                                    FILE_INFO_BY_HANDLE_CLASS file_info_class,
                                    LPVOID file_info,
                                    DWORD buffer_size) {
    return get_file_information_by_handle_ex_(
        handle, file_info_class, file_info, buffer_size);
  }

 private:
  using CancelIoExFunc = BOOL(WINAPI*)(HANDLE, LPOVERLAPPED);
  using GetFileInformationByHandleExFunc = BOOL(WINAPI*)(
      HANDLE, FILE_INFO_BY_HANDLE_CLASS, LPVOID, DWORD);

  bool is_vista_or_higher_;
  CancelIoExFunc cancel_io_ex_;
  GetFileInformationByHandleExFunc get_file_information_by_handle_ex_;
};

base::LazyInstance<VistaOrHigherFunctions> g_vista_or_higher_functions =
    LAZY_INSTANCE_INITIALIZER;

class RawChannelWin final : public RawChannel {
 public:
  RawChannelWin(ScopedPlatformHandle handle)
      : handle_(handle.Pass()), io_handler_(nullptr) {
    DCHECK(handle_.is_valid());
  }
  ~RawChannelWin() override {
    DCHECK(!io_handler_);
  }

 private:
  // RawChannelIOHandler receives OS notifications for I/O completion. Currently
  // this object is only used on the IO thread, other than ReleaseHandle. But
  // there's nothing preventing using this on other threads.
  //
  // It manages its own destruction. Destruction happens on the I/O thread when
  // all the following conditions are satisfied:
  //   - |DetachFromOwnerNoLock()| has been called;
  //   - there is no pending read;
  //   - there is no pending write.
  class RawChannelIOHandler {
   public:
    RawChannelIOHandler(RawChannelWin* owner,
                        ScopedPlatformHandle handle)
        : handle_(handle.Pass()),
          owner_(owner),
          suppress_self_destruct_(false),
          pending_read_(false),
          pending_write_(false),
          platform_handles_written_(0),
          read_event_(CreateEvent(NULL, FALSE, FALSE, NULL)),
          write_event_(CreateEvent(NULL, FALSE, FALSE, NULL)),
          read_wait_object_(NULL),
          write_wait_object_(NULL),
          read_event_signalled_(false),
          write_event_signalled_(false),
          message_loop_for_io_(base::MessageLoop::current()->task_runner()),
          weak_ptr_factory_(this) {
      memset(&read_context_.overlapped, 0, sizeof(read_context_.overlapped));
      memset(&write_context_.overlapped, 0, sizeof(write_context_.overlapped));
      read_context_.overlapped.hEvent = read_event_.Get();
      write_context_.overlapped.hEvent = write_event_.Get();

      this_weakptr_ = weak_ptr_factory_.GetWeakPtr();
      RegisterWaitForSingleObject(&read_wait_object_, read_event_.Get(),
          ReadCompleted, this, INFINITE, WT_EXECUTEINWAITTHREAD);
      RegisterWaitForSingleObject(&write_wait_object_, write_event_.Get(),
          WriteCompleted, this, INFINITE, WT_EXECUTEINWAITTHREAD);
    }

    ~RawChannelIOHandler() {
      if (read_wait_object_)
        UnregisterWaitEx(read_wait_object_, INVALID_HANDLE_VALUE);

      if (write_wait_object_)
        UnregisterWaitEx(write_wait_object_, INVALID_HANDLE_VALUE);
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

    // Instructs the object to wait for an OnObjectSignaled notification.
    void OnPendingReadStarted() {
      DCHECK(owner_);
      DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
      DCHECK(!pending_read_);
      pending_read_ = true;
      read_event_signalled_ = false;
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

    // Instructs the object to wait for an OnObjectSignaled notification.
    void OnPendingWriteStartedNoLock(size_t platform_handles_written) {
      DCHECK(owner_);
      owner_->write_lock().AssertAcquired();
      DCHECK(!pending_write_);
      pending_write_ = true;
      write_event_signalled_ = false;
      platform_handles_written_ = platform_handles_written;
    }

    // Must be called on the I/O thread under read and write locks.
    // After this call, the owner must not make any further calls on this
    // object, and therefore the object is used on the I/O thread exclusively
    // (if it stays alive).
    void DetachFromOwnerNoLock(scoped_ptr<ReadBuffer> read_buffer,
                               scoped_ptr<WriteBuffer> write_buffer) {
      DCHECK(owner_);
      DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
      owner_->read_lock().AssertAcquired();
      owner_->write_lock().AssertAcquired();

      // If read/write is pending, we have to retain the corresponding buffer.
      if (pending_read_)
        preserved_read_buffer_after_detach_ = read_buffer.Pass();
      if (pending_write_)
        preserved_write_buffer_after_detach_ = write_buffer.Pass();

      owner_ = nullptr;
      if (ShouldSelfDestruct())
        delete this;
    }

    ScopedPlatformHandle ReleaseHandle(
        std::vector<char>* serialized_read_buffer,
        std::vector<char>* serialized_write_buffer) {
      // Cancel pending IO calls.
      if (g_vista_or_higher_functions.Get().is_vista_or_higher()) {
        g_vista_or_higher_functions.Get().CancelIoEx(handle(), nullptr);
      } else {
        CHECK(false) << "TODO(jam): handle XP";
      }

      size_t additional_bytes_read = 0;
      if (pending_read_) {
        bool wait = false;
        UnregisterWaitEx(read_wait_object_, INVALID_HANDLE_VALUE);
        read_wait_object_ = NULL;
        if (!read_event_signalled_)
          wait = true;
        DWORD bytes_read_dword = 0;

        // Since we cancelled pending IO calls above, we need to know if the
        // read did succeed (i.e. it completed and there's a pending task posted
        // to alert us) or if it was cancelled. This important because if the
        // read completed, we don't want to serialize those bytes again.
        // TODO(jam): for XP, can return TRUE here to wait. also below.
        BOOL rv = GetOverlappedResult(
            handle(), &read_context_.overlapped, &bytes_read_dword,
            wait ? TRUE : FALSE);
        if (rv && read_context_.overlapped.Internal != STATUS_CANCELLED)
          additional_bytes_read = bytes_read_dword;
        pending_read_ = false;
      }

      RawChannel::WriteBuffer* write_buffer = owner_->write_buffer_no_lock();

      size_t additional_bytes_written = 0;
      size_t additional_platform_handles_written = 0;
      if (pending_write_) {
        bool wait = false;
        UnregisterWaitEx(write_wait_object_, INVALID_HANDLE_VALUE);
        write_wait_object_ = NULL;
        if (!write_event_signalled_)
          wait = true;

        DWORD bytes_written_dword = 0;

        // See comment above.
        BOOL rv = GetOverlappedResult(
            handle(), &write_context_.overlapped, &bytes_written_dword,
            wait ? TRUE : FALSE);

        if (rv && write_context_.overlapped.Internal != STATUS_CANCELLED) {
          CHECK(!write_buffer->IsEmpty());

          additional_bytes_written = static_cast<size_t>(bytes_written_dword);
          additional_platform_handles_written = platform_handles_written_;
          platform_handles_written_ = 0;
        }
        pending_write_ = false;
      }

      owner_->SerializeReadBuffer(
          additional_bytes_read, serialized_read_buffer);
      owner_->SerializeWriteBuffer(
          additional_bytes_written, additional_platform_handles_written,
          serialized_write_buffer);

      // There's a PostTask inside RawChannel because an error over the channel
      // occurred. We need to propagate this, otherwise the object using this
      // channel will never get a peer-closed signal.
      if (owner_->pending_error()) {
        handle_.reset();
        serialized_read_buffer->clear();
        serialized_write_buffer->clear();
        return ScopedPlatformHandle();
      }

      return ScopedPlatformHandle(handle_.release());
    }

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
      if (!owner_) {
        pending_read_ = false;
        return;
      }

      // Must acquire the read lock before we update pending_read_, since
      // otherwise there is a race condition in ReleaseHandle if this method
      // sets it to false but ReleaseHandle acquired read lock. It would then
      // think there's no pending read and miss the read bytes.
      base::AutoLock locker(owner_->read_lock());

      // This can happen if ReleaseHandle was called and it set pending_read to
      // false. We don't want to call owner_->OnReadCompletedNoLock since the
      // read_buffer has been freed.
      if (!pending_read_)
        return;

      CHECK(pending_read_);
      pending_read_ = false;

      // Note: |OnReadCompleted()| may detach us from |owner_|.
      if (error == ERROR_SUCCESS || error == ERROR_NO_MORE_ITEMS) {
        DCHECK_GT(bytes_read, 0u);
        owner_->OnReadCompletedNoLock(IO_SUCCEEDED, bytes_read);
      } else if (error == ERROR_BROKEN_PIPE  || error == STATUS_PIPE_BROKEN) {
        DCHECK_EQ(bytes_read, 0u);
        owner_->OnReadCompletedNoLock(IO_FAILED_SHUTDOWN, 0);
      } else {
        DCHECK_EQ(bytes_read, 0u);
        LOG(WARNING) << "ReadFile: " << logging::SystemErrorCodeToString(error);
        owner_->OnReadCompletedNoLock(IO_FAILED_UNKNOWN, 0);
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

      base::AutoLock locker(owner_->write_lock());
      // This can happen if ReleaseHandle was called and it set pending_write to
      // false. We don't want to call owner_->OnWriteCompletedNoLock since the
      // write_buffer has been freed.
      if (!pending_write_)
        return;

      CHECK(pending_write_);
      pending_write_ = false;

      // Note: |OnWriteCompleted()| may detach us from |owner_|.
      if (error == ERROR_SUCCESS || error == ERROR_NO_MORE_ITEMS) {
        // Reset |platform_handles_written_| before calling |OnWriteCompleted()|
        // since that function may call back to this class and set it again.
        size_t local_platform_handles_written = platform_handles_written_;
        platform_handles_written_ = 0;
        owner_->OnWriteCompletedNoLock(
            IO_SUCCEEDED, local_platform_handles_written, bytes_written);
      } else if (error == ERROR_BROKEN_PIPE  || error == STATUS_PIPE_BROKEN) {
        owner_->OnWriteCompletedNoLock(IO_FAILED_SHUTDOWN, 0, 0);
      } else {
        LOG(WARNING) << "WriteFile: "
                     << logging::SystemErrorCodeToString(error);
        owner_->OnWriteCompletedNoLock(IO_FAILED_UNKNOWN, 0, 0);
      }
    }

    void OnObjectSignaled(HANDLE object) {
      DCHECK(!owner_ ||
             base::MessageLoop::current() == owner_->message_loop_for_io());
      // Since this is called on the IO thread, no locks needed for owner_.
      bool handle_is_valid = false;
      if (owner_)
        owner_->read_lock().Acquire();
      handle_is_valid = handle_.is_valid();
      if (owner_)
        owner_->read_lock().Release();
      if (!handle_is_valid) {
        if (object == read_event_.Get())
          pending_read_ = false;
        else
          pending_write_ = false;
        if (ShouldSelfDestruct())
          delete this;
        return;
      }

      // Suppress self-destruction inside |OnReadCompleted()|, etc. (in case
      // they result in a call to |Shutdown()|).
      bool old_suppress_self_destruct = suppress_self_destruct_;
      suppress_self_destruct_ = true;
      if (object == read_event_.Get()) {
        OnReadCompleted(read_context_.overlapped.InternalHigh,
                        read_context_.overlapped.Internal);
      } else {
        CHECK(object == write_event_.Get());
        OnWriteCompleted(write_context_.overlapped.InternalHigh,
                         write_context_.overlapped.Internal);
      }

      // Maybe allow self-destruction again.
      suppress_self_destruct_ = old_suppress_self_destruct;

      if (ShouldSelfDestruct())
        delete this;
    }

    static void CALLBACK ReadCompleted(void* param, BOOLEAN timed_out) {
      DCHECK(!timed_out);
      // The destructor blocks on any callbacks that are in flight, so we know
      // that that is always a pointer to a valid RawChannelIOHandler.
      RawChannelIOHandler* that = static_cast<RawChannelIOHandler*>(param);
      that->read_event_signalled_ = true;
      that->message_loop_for_io_->PostTask(
          FROM_HERE,
          base::Bind(&RawChannelIOHandler::OnObjectSignaled,
                     that->this_weakptr_, that->read_event_.Get()));
    }

    static void CALLBACK WriteCompleted(void* param, BOOLEAN timed_out) {
      DCHECK(!timed_out);
      // The destructor blocks on any callbacks that are in flight, so we know
      // that that is always a pointer to a valid RawChannelIOHandler.
      RawChannelIOHandler* that = static_cast<RawChannelIOHandler*>(param);
      that->write_event_signalled_ = true;
      that->message_loop_for_io_->PostTask(
          FROM_HERE,
          base::Bind(&RawChannelIOHandler::OnObjectSignaled,
                     that->this_weakptr_, that->write_event_.Get()));
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

    base::win::ScopedHandle read_event_;
    base::win::ScopedHandle write_event_;

    HANDLE read_wait_object_;
    HANDLE write_wait_object_;

    // Since we use auto-reset event, these variables let ReleaseHandle know if
    // UnregisterWaitEx ended up running a callback or not.
    bool read_event_signalled_;
    bool write_event_signalled_;

    // These are used by the callbacks for the wait event watchers.
    scoped_refptr<base::SingleThreadTaskRunner> message_loop_for_io_;
    base::WeakPtr<RawChannelIOHandler> this_weakptr_;
    base::WeakPtrFactory<RawChannelIOHandler> weak_ptr_factory_;

    MOJO_DISALLOW_COPY_AND_ASSIGN(RawChannelIOHandler);
  };

  ScopedPlatformHandle ReleaseHandleNoLock(
      std::vector<char>* serialized_read_buffer,
      std::vector<char>* serialized_write_buffer) override {
    if (handle_.is_valid()) {
      // SetInitialBuffer could have been called on main thread before OnInit
      // is called on Io thread. and in meantime releasehandle called.
      SerializeReadBuffer(0u, serialized_read_buffer);

      // We could have been given messages to write before OnInit.
      SerializeWriteBuffer(0u, 0u, serialized_write_buffer);

      return ScopedPlatformHandle(PlatformHandle(handle_.release().handle));
    }

    return io_handler_->ReleaseHandle(serialized_read_buffer,
                                      serialized_write_buffer);
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

    // If the read is pending or the read has succeeded but we don't skip
    // completion port on success, instruct |io_handler_| to wait for the
    // completion packet.
    //
    // TODO(yzshen): It seems there isn't document saying that all error cases
    // (other than ERROR_IO_PENDING) are guaranteed to *not* queue a completion
    // packet. If we do get one for errors, OnObjectSignaled()| will crash so we
    // will learn about it.
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
    return Read(&bytes_read);
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

  size_t SerializePlatformHandles() override {
    if (!write_buffer_no_lock()->HavePlatformHandlesToSend())
      return 0u;

    // Since we're not sure which process might ultimately deserialize this
    // message, we can't duplicate the handle now. Instead, write the process
    // ID and handle now and let the receiver duplicate it.
    PlatformHandle* platform_handles;
    void* serialization_data_temp;
    size_t num_platform_handles;
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
    return num_platform_handles;
  }

  IOResult WriteNoLock(size_t* platform_handles_written,
                       size_t* bytes_written) override {
    write_lock().AssertAcquired();

    DCHECK(io_handler_);
    DCHECK(!io_handler_->pending_write_no_lock());

    size_t num_platform_handles = SerializePlatformHandles();

    std::vector<WriteBuffer::Buffer> buffers;
    write_buffer_no_lock()->GetBuffers(&buffers);
    DCHECK(!buffers.empty());

    // TODO(yzshen): Handle multi-segment writes more efficiently.
    DWORD bytes_written_dword = 0;

    BOOL result =
        WriteFile(io_handler_->handle(), buffers[0].addr,
                  static_cast<DWORD>(buffers[0].size), &bytes_written_dword,
                  &io_handler_->write_context_no_lock()->overlapped);
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

    // If the write is pending or the write has succeeded but we don't skip
    // completion port on success, instruct |io_handler_| to wait for the
    // completion packet.
    //
    // TODO(yzshen): it seems there isn't document saying that all error cases
    // (other than ERROR_IO_PENDING) are guaranteed to *not* queue a completion
    // packet. If we do get one for errors, OnObjectSignaled will crash so we
    // will learn about it.

    io_handler_->OnPendingWriteStartedNoLock(num_platform_handles);
    return IO_PENDING;
  }

  IOResult ScheduleWriteNoLock() override {
    write_lock().AssertAcquired();

    DCHECK(io_handler_);
    DCHECK(!io_handler_->pending_write_no_lock());

    size_t platform_handles_written = 0;
    size_t bytes_written = 0;
    return WriteNoLock(&platform_handles_written, &bytes_written);
  }

  void OnInit() override {
    DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

    if (!handle_.is_valid()) {
      LOG(ERROR) << "Note: RawChannelWin " << this
                 << " early exiting in OnInit because no handle";
      return;
    }

    DCHECK(handle_.is_valid());
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
      DCHECK(read_buffer->IsEmpty());
      DCHECK(write_buffer->IsEmpty());
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
  DCHECK_NE(other, this);
  PlatformHandle this_handle = HandleForDebuggingNoLock();
  PlatformHandle other_handle = other->HandleForDebuggingNoLock();

  if (g_vista_or_higher_functions.Get().is_vista_or_higher()) {
    WCHAR data1[_MAX_PATH + sizeof(FILE_NAME_INFO)];
    WCHAR data2[_MAX_PATH + sizeof(FILE_NAME_INFO)];
    FILE_NAME_INFO* fileinfo1 = reinterpret_cast<FILE_NAME_INFO *>(data1);
    FILE_NAME_INFO* fileinfo2 = reinterpret_cast<FILE_NAME_INFO *>(data2);
    CHECK(g_vista_or_higher_functions.Get().GetFileInformationByHandleEx(
        this_handle.handle, FileNameInfo, fileinfo1, arraysize(data1)));
    CHECK(g_vista_or_higher_functions.Get().GetFileInformationByHandleEx(
        other_handle.handle, FileNameInfo, fileinfo2, arraysize(data2)));
    std::wstring filepath1(fileinfo1->FileName, fileinfo1->FileNameLength / 2);
    std::wstring filepath2(fileinfo2->FileName, fileinfo2->FileNameLength / 2);
    return filepath1 == filepath2;
  } else {
    // TODO: XP: see http://stackoverflow.com/questions/65170/how-to-get-name-associated-with-open-handle/5286888#5286888
    CHECK(false) << "TODO(jam): handle XP";
    return false;
  }
}

}  // namespace edk
}  // namespace mojo
