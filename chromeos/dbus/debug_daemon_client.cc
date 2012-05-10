// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <unistd.h>

#include "chromeos/dbus/debug_daemon_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/chromeos/chromeos_version.h"
#include "base/eintr_wrapper.h"
#include "base/memory/ref_counted_memory.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/threading/worker_pool.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Used in DebugDaemonClient::EmptySystemStopTracingCallback().
void EmptyStopSystemTracingCallbackBody(
  const scoped_refptr<base::RefCountedString>& unused_result) {
}

// Simple class to encapsulate collecting data from a pipe into a
// string.  To use, instantiate the class, start i/o, and then delete
// the instance on callback.  The data should be retrieved before
// delete and extracted or copied.
//
// TODO(sleffler) move data collection to a sub-class so this
// can be reused to process data as it is received
class PipeReader {
 public:
  typedef base::Callback<void(void)>IOCompleteCallback;

  explicit PipeReader(IOCompleteCallback callback)
      : data_stream_(NULL),
        io_buffer_(new net::IOBufferWithSize(4096)),
        weak_ptr_factory_(this),
        callback_(callback) {
    pipe_fd_[0] = pipe_fd_[1] = -1;
  }

  virtual ~PipeReader() {
    if (pipe_fd_[0] != -1)
      if (HANDLE_EINTR(close(pipe_fd_[0])) < 0)
        PLOG(ERROR) << "close[0]";
    if (pipe_fd_[1] != -1)
      if (HANDLE_EINTR(close(pipe_fd_[1])) < 0)
        PLOG(ERROR) << "close[1]";
  }

  // Returns descriptor for the writeable side of the pipe.
  int GetWriteFD() { return pipe_fd_[1]; }

  // Closes writeable descriptor; normally used in parent process after fork.
  void CloseWriteFD() {
    if (pipe_fd_[1] != -1) {
      if (HANDLE_EINTR(close(pipe_fd_[1])) < 0)
        PLOG(ERROR) << "close";
      pipe_fd_[1] = -1;
    }
  }

  // Returns collected data.
  std::string* data() { return &data_; }

  // Starts data collection.  Returns true if stream was setup correctly.
  // On success data will automatically be accumulated into a string that
  // can be retrieved with PipeReader::data().  To shutdown collection delete
  // the instance and/or use PipeReader::OnDataReady(-1).
  bool StartIO() {
    // Use a pipe to collect data
    const int status = HANDLE_EINTR(pipe(pipe_fd_));
    if (status < 0) {
      PLOG(ERROR) << "pipe";
      return false;
    }
    base::PlatformFile data_file_ = pipe_fd_[0];  // read side
    data_stream_.reset(new net::FileStream(data_file_,
        base::PLATFORM_FILE_READ | base::PLATFORM_FILE_ASYNC,
        NULL));

    // Post an initial async read to setup data collection
    int rv = data_stream_->Read(io_buffer_.get(), io_buffer_->size(),
        base::Bind(&PipeReader::OnDataReady, weak_ptr_factory_.GetWeakPtr()));
    if (rv != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Unable to post initial read";
      return false;
    }
    return true;
  }

  // Called when pipe data are available.  Can also be used to shutdown
  // data collection by passing -1 for |byte_count|.
  void OnDataReady(int byte_count) {
    DVLOG(1) << "OnDataReady byte_count " << byte_count;
    if (byte_count <= 0) {
      callback_.Run();  // signal creator to take data and delete us
      return;
    }
    data_.append(io_buffer_->data(), byte_count);

    // Post another read
    int rv = data_stream_->Read(io_buffer_.get(), io_buffer_->size(),
        base::Bind(&PipeReader::OnDataReady, weak_ptr_factory_.GetWeakPtr()));
    if (rv != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Unable to post another read";
      // TODO(sleffler) do something more intelligent?
    }
  }

 private:
  friend class base::RefCounted<PipeReader>;

  int pipe_fd_[2];
  scoped_ptr<net::FileStream> data_stream_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  base::WeakPtrFactory<PipeReader> weak_ptr_factory_;
  std::string data_;
  IOCompleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PipeReader);
};

}  // namespace

namespace chromeos {

// The DebugDaemonClient implementation used in production.
class DebugDaemonClientImpl : public DebugDaemonClient {
 public:
  explicit DebugDaemonClientImpl(dbus::Bus* bus)
      : debugdaemon_proxy_(NULL),
        weak_ptr_factory_(this),
        pipe_reader_(NULL) {
    debugdaemon_proxy_ = bus->GetObjectProxy(
        debugd::kDebugdServiceName,
        dbus::ObjectPath(debugd::kDebugdServicePath));
  }

  virtual ~DebugDaemonClientImpl() {}

  // DebugDaemonClient override.
  virtual void GetDebugLogs(base::PlatformFile file,
                            const GetDebugLogsCallback& callback) OVERRIDE {

    dbus::FileDescriptor* file_descriptor = new dbus::FileDescriptor(file);
    // Punt descriptor validity check to a worker thread; on return we'll
    // issue the D-Bus request to stop tracing and collect results.
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&DebugDaemonClientImpl::CheckValidity,
                   file_descriptor),
        base::Bind(&DebugDaemonClientImpl::OnCheckValidityGetDebugLogs,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(file_descriptor),
                   callback),
        false);
  }

  virtual void SetDebugMode(const std::string& subsystem,
                            const SetDebugModeCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetDebugMode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(subsystem);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnSetDebugMode,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void StartSystemTracing() OVERRIDE {
    dbus::MethodCall method_call(
        debugd::kDebugdInterface,
        debugd::kSystraceStart);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("all"); // TODO(sleffler) parameterize category list

    DVLOG(1) << "Requesting a systrace start";
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnStartSystemTracing,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual bool RequestStopSystemTracing(const StopSystemTracingCallback&
      callback) OVERRIDE {
    if (pipe_reader_ != NULL) {
      LOG(ERROR) << "Busy doing StopSystemTracing";
      return false;
    }

    pipe_reader_.reset(new PipeReader(
        base::Bind(&DebugDaemonClientImpl::OnIOComplete,
                   weak_ptr_factory_.GetWeakPtr())));
    int write_fd = -1;
    if (!pipe_reader_->StartIO()) {
      LOG(ERROR) << "Cannot create pipe reader";
      // NB: continue anyway to shutdown tracing; toss trace data
      write_fd = HANDLE_EINTR(open("/dev/null", O_WRONLY));
      // TODO(sleffler) if this fails AppendFileDescriptor will abort
    } else {
      write_fd = pipe_reader_->GetWriteFD();
    }

    dbus::FileDescriptor* file_descriptor = new dbus::FileDescriptor(write_fd);
    // Punt descriptor validity check to a worker thread; on return we'll
    // issue the D-Bus request to stop tracing and collect results.
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&DebugDaemonClientImpl::CheckValidity,
                   file_descriptor),
        base::Bind(&DebugDaemonClientImpl::OnCheckValidityRequestStopSystem,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(file_descriptor),
                   callback),
        false);

    return true;
  }

 private:
  // Called to check descriptor validity on a thread where i/o is permitted.
  static void CheckValidity(dbus::FileDescriptor* file_descriptor) {
    file_descriptor->CheckValidity();
  }

  // Called when a CheckValidity response is received.
  void OnCheckValidityGetDebugLogs(dbus::FileDescriptor* file_descriptor,
                                   const GetDebugLogsCallback& callback) {
    // Issue the dbus request to get debug logs.
    dbus::MethodCall method_call(
        debugd::kDebugdInterface,
        debugd::kGetDebugLogs);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(*file_descriptor);

    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetDebugLogs,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // Called when a response for GetDebugLogs() is received.
  void OnGetDebugLogs(const GetDebugLogsCallback& callback,
                      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to get debug logs";
      callback.Run(false);
      return;
    }
    callback.Run(true);
  }

  // Called when a response for SetDebugMode() is received.
  void OnSetDebugMode(const SetDebugModeCallback& callback,
                      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to change debug mode";
      callback.Run(false);
    } else {
      callback.Run(true);
    }
  }

  // Called when a response for StartSystemTracing() is received.
  void OnStartSystemTracing(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request systrace start";
      return;
    }
  }

  // Called when a CheckValidity response is received.
  void OnCheckValidityRequestStopSystem(
      dbus::FileDescriptor* file_descriptor,
      const StopSystemTracingCallback& callback) {
    // Issue the dbus request to stop system tracing
    dbus::MethodCall method_call(
        debugd::kDebugdInterface,
        debugd::kSystraceStop);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(*file_descriptor);

    callback_ = callback;

    DVLOG(1) << "Requesting a systrace stop";
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnRequestStopSystemTracing,
                   weak_ptr_factory_.GetWeakPtr()));

    pipe_reader_->CloseWriteFD();  // close our copy of fd after send
  }

  // Called when a response for RequestStopSystemTracing() is received.
  void OnRequestStopSystemTracing(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request systrace stop";
      pipe_reader_->OnDataReady(-1); // terminate data stream
    }
    // NB: requester is signaled when i/o completes
  }

  // Called when pipe i/o completes; pass data on and delete the instance.
  void OnIOComplete() {
    callback_.Run(base::RefCountedString::TakeString(pipe_reader_->data()));
    pipe_reader_.reset();
  }

  dbus::ObjectProxy* debugdaemon_proxy_;
  base::WeakPtrFactory<DebugDaemonClientImpl> weak_ptr_factory_;
  scoped_ptr<PipeReader> pipe_reader_;
  StopSystemTracingCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DebugDaemonClientImpl);
};

// The DebugDaemonClient implementation used on Linux desktop,
// which does nothing.
class DebugDaemonClientStubImpl : public DebugDaemonClient {
  // DebugDaemonClient overrides.
  virtual void GetDebugLogs(base::PlatformFile file,
                            const GetDebugLogsCallback& callback) OVERRIDE {
    callback.Run(false);
  }
  virtual void SetDebugMode(const std::string& subsystem,
                            const SetDebugModeCallback& callback) OVERRIDE {
    callback.Run(false);
  }
  virtual void StartSystemTracing() OVERRIDE {}
  virtual bool RequestStopSystemTracing(const StopSystemTracingCallback&
      callback) OVERRIDE {
    std::string no_data;
    callback.Run(base::RefCountedString::TakeString(&no_data));
    return true;
  }
};

DebugDaemonClient::DebugDaemonClient() {
}

DebugDaemonClient::~DebugDaemonClient() {
}

// static
DebugDaemonClient::StopSystemTracingCallback
DebugDaemonClient::EmptyStopSystemTracingCallback() {
  return base::Bind(&EmptyStopSystemTracingCallbackBody);
}

// static
DebugDaemonClient* DebugDaemonClient::Create(DBusClientImplementationType type,
                                   dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new DebugDaemonClientImpl(bus);
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new DebugDaemonClientStubImpl();
}

}  // namespace chromeos
