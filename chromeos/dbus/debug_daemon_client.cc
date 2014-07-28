// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/debug_daemon_client.h"

#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "chromeos/dbus/pipe_reader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Used in DebugDaemonClient::EmptySystemStopTracingCallback().
void EmptyStopSystemTracingCallbackBody(
  const scoped_refptr<base::RefCountedString>& unused_result) {
}

}  // namespace

namespace chromeos {

// The DebugDaemonClient implementation used in production.
class DebugDaemonClientImpl : public DebugDaemonClient {
 public:
  DebugDaemonClientImpl() : debugdaemon_proxy_(NULL), weak_ptr_factory_(this) {}

  virtual ~DebugDaemonClientImpl() {}

  // DebugDaemonClient override.
  virtual void DumpDebugLogs(bool is_compressed,
                             base::File file,
                             scoped_refptr<base::TaskRunner> task_runner,
                             const GetDebugLogsCallback& callback) OVERRIDE {
    dbus::FileDescriptor* file_descriptor = new dbus::FileDescriptor;
    file_descriptor->PutValue(file.TakePlatformFile());
    // Punt descriptor validity check to a worker thread; on return we'll
    // issue the D-Bus request to stop tracing and collect results.
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&dbus::FileDescriptor::CheckValidity,
                   base::Unretained(file_descriptor)),
        base::Bind(&DebugDaemonClientImpl::OnCheckValidityGetDebugLogs,
                   weak_ptr_factory_.GetWeakPtr(),
                   is_compressed,
                   base::Owned(file_descriptor),
                   callback));
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

  virtual void GetRoutes(bool numeric, bool ipv6,
                         const GetRoutesCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetRoutes);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter sub_writer(NULL);
    writer.OpenArray("{sv}", &sub_writer);
    dbus::MessageWriter elem_writer(NULL);
    sub_writer.OpenDictEntry(&elem_writer);
    elem_writer.AppendString("numeric");
    elem_writer.AppendVariantOfBool(numeric);
    sub_writer.CloseContainer(&elem_writer);
    sub_writer.OpenDictEntry(&elem_writer);
    elem_writer.AppendString("v6");
    elem_writer.AppendVariantOfBool(ipv6);
    sub_writer.CloseContainer(&elem_writer);
    writer.CloseContainer(&sub_writer);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetRoutes,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetNetworkStatus(const GetNetworkStatusCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetNetworkStatus);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetNetworkStatus,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetModemStatus(const GetModemStatusCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetModemStatus);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetModemStatus,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetWiMaxStatus(const GetWiMaxStatusCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetWiMaxStatus);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetWiMaxStatus,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetNetworkInterfaces(
      const GetNetworkInterfacesCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetInterfaces);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetNetworkInterfaces,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetPerfData(uint32_t duration,
                           const GetPerfDataCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetRichPerfData);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(duration);

    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetPerfData,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetScrubbedLogs(const GetLogsCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetFeedbackLogs);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetAllLogs,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetAllLogs(const GetLogsCallback& callback)
      OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetAllLogs);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetAllLogs,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void GetUserLogFiles(
      const GetLogsCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetUserLogFiles);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnGetUserLogFiles,
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
        base::Bind(&DebugDaemonClientImpl::OnStartMethod,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual bool RequestStopSystemTracing(
      scoped_refptr<base::TaskRunner> task_runner,
      const StopSystemTracingCallback& callback) OVERRIDE {
    if (pipe_reader_ != NULL) {
      LOG(ERROR) << "Busy doing StopSystemTracing";
      return false;
    }

    pipe_reader_.reset(new PipeReaderForString(
        task_runner,
        base::Bind(&DebugDaemonClientImpl::OnIOComplete,
                   weak_ptr_factory_.GetWeakPtr())));

    base::File pipe_write_end = pipe_reader_->StartIO();
    // Create dbus::FileDescriptor on the worker thread; on return we'll
    // issue the D-Bus request to stop tracing and collect results.
    base::PostTaskAndReplyWithResult(
        task_runner,
        FROM_HERE,
        base::Bind(
            &DebugDaemonClientImpl::CreateFileDescriptorToStopSystemTracing,
            base::Passed(&pipe_write_end)),
        base::Bind(
            &DebugDaemonClientImpl::OnCreateFileDescriptorRequestStopSystem,
            weak_ptr_factory_.GetWeakPtr(),
            callback));
    return true;
  }

  virtual void TestICMP(const std::string& ip_address,
                        const TestICMPCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kTestICMP);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip_address);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnTestICMP,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void TestICMPWithOptions(
      const std::string& ip_address,
      const std::map<std::string, std::string>& options,
      const TestICMPCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kTestICMPWithOptions);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter sub_writer(NULL);
    dbus::MessageWriter elem_writer(NULL);

    // Write the host.
    writer.AppendString(ip_address);

    // Write the options.
    writer.OpenArray("{ss}", &sub_writer);
    std::map<std::string, std::string>::const_iterator it;
    for (it = options.begin(); it != options.end(); ++it) {
      sub_writer.OpenDictEntry(&elem_writer);
      elem_writer.AppendString(it->first);
      elem_writer.AppendString(it->second);
      sub_writer.CloseContainer(&elem_writer);
    }
    writer.CloseContainer(&sub_writer);

    // Call the function.
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnTestICMP,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  virtual void UploadCrashes() OVERRIDE {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kUploadCrashes);
    debugdaemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&DebugDaemonClientImpl::OnStartMethod,
                   weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    debugdaemon_proxy_ =
        bus->GetObjectProxy(debugd::kDebugdServiceName,
                            dbus::ObjectPath(debugd::kDebugdServicePath));
  }

 private:
  // Called when a CheckValidity response is received.
  void OnCheckValidityGetDebugLogs(bool is_compressed,
                                   dbus::FileDescriptor* file_descriptor,
                                   const GetDebugLogsCallback& callback) {
    // Issue the dbus request to get debug logs.
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kDumpDebugLogs);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(is_compressed);
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

  void OnGetRoutes(const GetRoutesCallback& callback,
                   dbus::Response* response) {
    std::vector<std::string> routes;
    if (response) {
      dbus::MessageReader reader(response);
      if (reader.PopArrayOfStrings(&routes)) {
        callback.Run(true, routes);
      } else {
        LOG(ERROR) << "Got non-array response from GetRoutes";
        callback.Run(false, routes);
      }
    } else {
      callback.Run(false, routes);
    }
  }

  void OnGetNetworkStatus(const GetNetworkStatusCallback& callback,
                          dbus::Response* response) {
    std::string status;
    if (response && dbus::MessageReader(response).PopString(&status))
      callback.Run(true, status);
    else
      callback.Run(false, "");
  }

  void OnGetModemStatus(const GetModemStatusCallback& callback,
                        dbus::Response* response) {
    std::string status;
    if (response && dbus::MessageReader(response).PopString(&status))
      callback.Run(true, status);
    else
      callback.Run(false, "");
  }

  void OnGetWiMaxStatus(const GetWiMaxStatusCallback& callback,
                        dbus::Response* response) {
    std::string status;
    if (response && dbus::MessageReader(response).PopString(&status))
      callback.Run(true, status);
    else
      callback.Run(false, "");
  }

  void OnGetNetworkInterfaces(const GetNetworkInterfacesCallback& callback,
                              dbus::Response* response) {
    std::string status;
    if (response && dbus::MessageReader(response).PopString(&status))
      callback.Run(true, status);
    else
      callback.Run(false, "");
  }

  void OnGetPerfData(const GetPerfDataCallback& callback,
                     dbus::Response* response) {
    std::vector<uint8> data;

    if (!response) {
      return;
    }

    dbus::MessageReader reader(response);
    const uint8* buffer = NULL;
    size_t buf_size = 0;
    if (!reader.PopArrayOfBytes(&buffer, &buf_size))
      return;

    // TODO(asharif): Figure out a way to avoid this copy.
    data.insert(data.end(), buffer, buffer + buf_size);

    callback.Run(data);
  }

  void OnGetAllLogs(const GetLogsCallback& callback,
                    dbus::Response* response) {
    std::map<std::string, std::string> logs;
    bool broken = false; // did we see a broken (k,v) pair?
    dbus::MessageReader sub_reader(NULL);
    if (!response || !dbus::MessageReader(response).PopArray(&sub_reader)) {
      callback.Run(false, logs);
      return;
    }
    while (sub_reader.HasMoreData()) {
      dbus::MessageReader sub_sub_reader(NULL);
      std::string key, value;
      if (!sub_reader.PopDictEntry(&sub_sub_reader)
          || !sub_sub_reader.PopString(&key)
          || !sub_sub_reader.PopString(&value)) {
        broken = true;
        break;
      }
      logs[key] = value;
    }
    callback.Run(!sub_reader.HasMoreData() && !broken, logs);
  }

  void OnGetUserLogFiles(const GetLogsCallback& callback,
                         dbus::Response* response) {
    return OnGetAllLogs(callback, response);
  }

  // Called when a response for a simple start is received.
  void OnStartMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request start";
      return;
    }
  }

  // Creates dbus::FileDescriptor from base::File.
  static scoped_ptr<dbus::FileDescriptor>
  CreateFileDescriptorToStopSystemTracing(base::File pipe_write_end) {
    if (!pipe_write_end.IsValid()) {
      LOG(ERROR) << "Cannot create pipe reader";
      // NB: continue anyway to shutdown tracing; toss trace data
      pipe_write_end.Initialize(base::FilePath(FILE_PATH_LITERAL("/dev/null")),
                                base::File::FLAG_OPEN | base::File::FLAG_WRITE);
      // TODO(sleffler) if this fails AppendFileDescriptor will abort
    }
    scoped_ptr<dbus::FileDescriptor> file_descriptor(new dbus::FileDescriptor);
    file_descriptor->PutValue(pipe_write_end.TakePlatformFile());
    file_descriptor->CheckValidity();
    return file_descriptor.Pass();
  }

  // Called when a CheckValidity response is received.
  void OnCreateFileDescriptorRequestStopSystem(
      const StopSystemTracingCallback& callback,
      scoped_ptr<dbus::FileDescriptor> file_descriptor) {
    DCHECK(file_descriptor);

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
  }

  // Called when a response for RequestStopSystemTracing() is received.
  void OnRequestStopSystemTracing(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request systrace stop";
      // If debugd crashes or completes I/O before this message is processed
      // then pipe_reader_ can be NULL, see OnIOComplete().
      if (pipe_reader_.get())
        pipe_reader_->OnDataReady(-1); // terminate data stream
    }
    // NB: requester is signaled when i/o completes
  }

  void OnTestICMP(const TestICMPCallback& callback, dbus::Response* response) {
    std::string status;
    if (response && dbus::MessageReader(response).PopString(&status))
      callback.Run(true, status);
    else
      callback.Run(false, "");
  }

  // Called when pipe i/o completes; pass data on and delete the instance.
  void OnIOComplete() {
    std::string pipe_data;
    pipe_reader_->GetData(&pipe_data);
    callback_.Run(base::RefCountedString::TakeString(&pipe_data));
    pipe_reader_.reset();
  }

  dbus::ObjectProxy* debugdaemon_proxy_;
  scoped_ptr<PipeReaderForString> pipe_reader_;
  StopSystemTracingCallback callback_;
  base::WeakPtrFactory<DebugDaemonClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DebugDaemonClientImpl);
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
DebugDaemonClient* DebugDaemonClient::Create() {
  return new DebugDaemonClientImpl();
}

}  // namespace chromeos
