// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"

#include <dbus/dbus-protocol.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_config.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/pipe_reader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

const char kCrOSTracingAgentName[] = "cros";
const char kCrOSTraceLabel[] = "systemTraceEvents";

// Because the cheets logs are very huge, we set the D-Bus timeout to 2 minutes.
const int kBigLogsDBusTimeoutMS = 120 * 1000;

// A self-deleting object that wraps the pipe reader operations for reading the
// big feedback logs. It will delete itself once the pipe stream has been
// terminated. Once the data has been completely read from the pipe, it invokes
// the GetLogsCallback |callback| passing the deserialized logs data back to
// the requester.
class PipeReaderWrapper : public base::SupportsWeakPtr<PipeReaderWrapper> {
 public:
  explicit PipeReaderWrapper(DebugDaemonClient::GetLogsCallback callback)
      : pipe_reader_(base::ThreadPool::CreateTaskRunner(
            {base::MayBlock(),
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
        callback_(std::move(callback)) {}

  base::ScopedFD Initialize() {
    return pipe_reader_.StartIO(
        base::BindOnce(&PipeReaderWrapper::OnIOComplete, AsWeakPtr()));
  }

  void OnIOComplete(base::Optional<std::string> result) {
    if (!result.has_value()) {
      VLOG(1) << "Failed to read data.";
      RunCallbackAndDestroy(base::nullopt);
      return;
    }

    JSONStringValueDeserializer json_reader(result.value());
    std::unique_ptr<base::DictionaryValue> logs =
        base::DictionaryValue::From(json_reader.Deserialize(nullptr, nullptr));
    if (!logs.get()) {
      VLOG(1) << "Failed to deserialize the JSON logs.";
      RunCallbackAndDestroy(base::nullopt);
      return;
    }

    std::map<std::string, std::string> data;
    for (const auto& entry : *logs)
      data[entry.first] = entry.second->GetString();
    RunCallbackAndDestroy(std::move(data));
  }

  void TerminateStream() {
    VLOG(1) << "Terminated";
    RunCallbackAndDestroy(base::nullopt);
  }

 private:
  void RunCallbackAndDestroy(
      base::Optional<std::map<std::string, std::string>> result) {
    if (result.has_value()) {
      std::move(callback_).Run(true, std::move(result.value()));
    } else {
      std::move(callback_).Run(false, std::map<std::string, std::string>());
    }
    delete this;
  }

  PipeReader pipe_reader_;
  DebugDaemonClient::GetLogsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PipeReaderWrapper);
};

// Convert the string representation of a D-Bus error into a
// DbusLibraryError value.
DbusLibraryError DbusLibraryErrorFromString(
    const std::string& dbus_error_string) {
  static const base::NoDestructor<std::map<std::string, DbusLibraryError>>
      error_string_map({
          {DBUS_ERROR_NO_REPLY, DbusLibraryError::kNoReply},
          {DBUS_ERROR_TIMEOUT, DbusLibraryError::kTimeout},
      });

  auto it = error_string_map->find(dbus_error_string);
  return it != error_string_map->end() ? it->second
                                       : DbusLibraryError::kGenericError;
}

}  // namespace

// The DebugDaemonClient implementation used in production.
class DebugDaemonClientImpl : public DebugDaemonClient {
 public:
  DebugDaemonClientImpl() : debugdaemon_proxy_(nullptr) {}

  ~DebugDaemonClientImpl() override = default;

  // DebugDaemonClient override.
  void DumpDebugLogs(bool is_compressed,
                     int file_descriptor,
                     VoidDBusMethodCallback callback) override {
    // Issue the dbus request to get debug logs.
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kDumpDebugLogs);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(is_compressed);
    writer.AppendFileDescriptor(file_descriptor);
    debugdaemon_proxy_->CallMethod(
        &method_call, kBigLogsDBusTimeoutMS,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetDebugMode(const std::string& subsystem,
                    VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetDebugMode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(subsystem);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetRoutes(
      bool numeric,
      bool ipv6,
      DBusMethodCallback<std::vector<std::string>> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kGetRoutes);
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
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnGetRoutes,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNetworkStatus(DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetNetworkStatus);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNetworkInterfaces(DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetInterfaces);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetPerfOutput(base::TimeDelta duration,
                     const std::vector<std::string>& perf_args,
                     int file_descriptor,
                     DBusMethodCallback<uint64_t> callback) override {
    DCHECK(file_descriptor);
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetPerfOutputFd);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(duration.InSeconds());
    writer.AppendArrayOfStrings(perf_args);
    writer.AppendFileDescriptor(file_descriptor);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnUint64Method,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopPerf(uint64_t session_id, VoidDBusMethodCallback callback) override {
    DCHECK(session_id);
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kStopPerf);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(session_id);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetScrubbedBigLogs(const cryptohome::AccountIdentifier& id,
                          GetLogsCallback callback) override {
    // The PipeReaderWrapper is a self-deleting object; we don't have to worry
    // about ownership or lifetime. We need to create a new one for each Big
    // Logs requests in order to queue these requests. One request can take a
    // long time to be processed and a new request should never be ignored nor
    // cancels the on-going one.
    PipeReaderWrapper* pipe_reader = new PipeReaderWrapper(std::move(callback));
    base::ScopedFD pipe_write_end = pipe_reader->Initialize();

    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetBigFeedbackLogs);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(pipe_write_end.get());
    writer.AppendString(id.account_id());

    DVLOG(1) << "Requesting big feedback logs";
    debugdaemon_proxy_->CallMethod(
        &method_call, kBigLogsDBusTimeoutMS,
        base::BindOnce(&DebugDaemonClientImpl::OnBigFeedbackLogsResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       pipe_reader->AsWeakPtr()));
  }

  void BackupArcBugReport(const std::string& userhash,
                          VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kBackupArcBugReport);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(userhash);

    DVLOG(1) << "Backing up ARC bug report";
    debugdaemon_proxy_->CallMethod(
        &method_call, kBigLogsDBusTimeoutMS,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetAllLogs(GetLogsCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kGetAllLogs);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnGetAllLogs,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetLog(const std::string& log_name,
              DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kGetLog);
    dbus::MessageWriter(&method_call).AppendString(log_name);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // base::trace_event::TracingAgent implementation.
  std::string GetTracingAgentName() override { return kCrOSTracingAgentName; }

  std::string GetTraceEventLabel() override { return kCrOSTraceLabel; }

  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         StartAgentTracingCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSystraceStart);
    dbus::MessageWriter writer(&method_call);
    if (trace_config.systrace_events().empty()) {
      writer.AppendString("all");  // TODO(sleffler) parameterize category list
    } else {
      std::string events;
      for (const std::string& event : trace_config.systrace_events()) {
        if (!events.empty())
          events += " ";
        events += event;
      }
      writer.AppendString(events);
    }

    DVLOG(1) << "Requesting a systrace start";
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStartMethod,
                       weak_ptr_factory_.GetWeakPtr()));

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetTracingAgentName(),
                                  true /* success */));
  }

  void StopAgentTracing(StopAgentTracingCallback callback) override {
    DCHECK(stop_agent_tracing_task_runner_);
    if (pipe_reader_ != NULL) {
      LOG(ERROR) << "Busy doing StopSystemTracing";
      return;
    }

    pipe_reader_ =
        std::make_unique<PipeReader>(stop_agent_tracing_task_runner_);
    callback_ = std::move(callback);
    base::ScopedFD pipe_write_end = pipe_reader_->StartIO(base::BindOnce(
        &DebugDaemonClientImpl::OnIOComplete, weak_ptr_factory_.GetWeakPtr()));

    DCHECK(pipe_write_end.is_valid());
    // Issue the dbus request to stop system tracing
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSystraceStop);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(pipe_write_end.get());

    DVLOG(1) << "Requesting a systrace stop";
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStopAgentTracing,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetStopAgentTracingTaskRunner(
      scoped_refptr<base::TaskRunner> task_runner) override {
    stop_agent_tracing_task_runner_ = task_runner;
  }

  void TestICMP(const std::string& ip_address,
                TestICMPCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kTestICMP);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip_address);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnTestICMP,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void TestICMPWithOptions(const std::string& ip_address,
                           const std::map<std::string, std::string>& options,
                           TestICMPCallback callback) override {
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
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnTestICMP,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UploadCrashes() override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kUploadCrashes);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStartMethod,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void EnableDebuggingFeatures(const std::string& password,
                               EnableDebuggingCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kEnableChromeDevFeatures);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(password);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnEnableDebuggingFeatures,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void QueryDebuggingFeatures(QueryDevFeaturesCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kQueryDevFeatures);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnQueryDebuggingFeatures,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RemoveRootfsVerification(EnableDebuggingCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kRemoveRootfsVerification);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnRemoveRootfsVerification,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override {
    debugdaemon_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void SetOomScoreAdj(const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
                      SetOomScoreAdjCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetOomScoreAdj);
    dbus::MessageWriter writer(&method_call);

    dbus::MessageWriter sub_writer(nullptr);
    writer.OpenArray("{ii}", &sub_writer);

    dbus::MessageWriter elem_writer(nullptr);
    for (const auto& entry : pid_to_oom_score_adj) {
      sub_writer.OpenDictEntry(&elem_writer);
      elem_writer.AppendInt32(entry.first);
      elem_writer.AppendInt32(entry.second);
      sub_writer.CloseContainer(&elem_writer);
    }
    writer.CloseContainer(&sub_writer);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetOomScoreAdj,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& ppd_contents,
      DebugDaemonClient::CupsAddPrinterCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kCupsAddManuallyConfiguredPrinter);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(uri);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(ppd_contents.data()),
        ppd_contents.size());

    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnPrinterAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsAddAutoConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      DebugDaemonClient::CupsAddPrinterCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kCupsAddAutoConfiguredPrinter);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(uri);

    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnPrinterAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsRemovePrinter(const std::string& name,
                         DebugDaemonClient::CupsRemovePrinterCallback callback,
                         base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kCupsRemovePrinter);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnPrinterRemoved,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void StartConcierge(ConciergeCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kStartVmConcierge);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStartConcierge,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopConcierge(ConciergeCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kStopVmConcierge);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStopConcierge,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartPluginVmDispatcher(const std::string& owner_id,
                               const std::string& lang,
                               PluginVmDispatcherCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kStartVmPluginDispatcher);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(owner_id);
    writer.AppendString(lang);
    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStartPluginVmDispatcher,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       owner_id));
  }

  void StopPluginVmDispatcher(PluginVmDispatcherCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kStopVmPluginDispatcher);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStopPluginVmDispatcher,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetRlzPingSent(SetRlzPingSentCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetRlzPingSent);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetRlzPingSent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetKstaledRatio(uint8_t val, KstaledRatioCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kKstaledSetRatio);
    dbus::MessageWriter writer(&method_call);
    writer.AppendByte(val);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetKstaledRatio,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetSchedulerConfigurationV2(
      const std::string& config_name,
      bool lock_policy,
      SetSchedulerConfigurationV2Callback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetSchedulerConfigurationV2);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(config_name);
    writer.AppendBool(lock_policy);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetSchedulerConfigurationV2,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetU2fFlags(const std::set<std::string>& flags,
                   VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetU2fFlags);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(base::JoinString(
        std::vector<std::string>(flags.begin(), flags.end()), ","));
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetU2fFlags(
      DBusMethodCallback<std::set<std::string>> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetU2fFlags);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnGetU2fFlags,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetSwapParameter(const std::string& parameter,
                        int32_t value,
                        DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, "SwapSetParameter");
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(parameter);
    writer.AppendInt32(value);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetSwapParameter,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    debugdaemon_proxy_ =
        bus->GetObjectProxy(debugd::kDebugdServiceName,
                            dbus::ObjectPath(debugd::kDebugdServicePath));
  }

 private:
  void OnGetRoutes(DBusMethodCallback<std::vector<std::string>> callback,
                   dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::vector<std::string> routes;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfStrings(&routes)) {
      LOG(ERROR) << "Got non-array response from GetRoutes";
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(routes));
  }

  void OnSetSwapParameter(DBusMethodCallback<std::string> callback,
                          dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::string res;
    dbus::MessageReader reader(response);
    if (!reader.PopString(&res)) {
      LOG(ERROR) << "Received a non-string response from dbus";
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(res));
  }

  void OnGetAllLogs(GetLogsCallback callback, dbus::Response* response) {
    std::map<std::string, std::string> logs;
    bool broken = false;  // did we see a broken (k,v) pair?
    dbus::MessageReader sub_reader(NULL);
    if (!response || !dbus::MessageReader(response).PopArray(&sub_reader)) {
      std::move(callback).Run(false, logs);
      return;
    }
    while (sub_reader.HasMoreData()) {
      dbus::MessageReader sub_sub_reader(NULL);
      std::string key, value;
      if (!sub_reader.PopDictEntry(&sub_sub_reader) ||
          !sub_sub_reader.PopString(&key) ||
          !sub_sub_reader.PopString(&value)) {
        broken = true;
        break;
      }
      logs[key] = value;
    }
    std::move(callback).Run(!sub_reader.HasMoreData() && !broken, logs);
  }

  void OnBigFeedbackLogsResponse(base::WeakPtr<PipeReaderWrapper> pipe_reader,
                                 dbus::Response* response) {
    if (!response && pipe_reader.get()) {
      // We need to terminate the data stream if an error occurred while the
      // pipe reader is still waiting on read.
      pipe_reader->TerminateStream();
    }
  }

  // Called when a response for a simple start is received.
  void OnStartMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request start";
      return;
    }
  }

  // Called when D-Bus method call which does not return the result is
  // completed or on its error.
  void OnVoidMethod(VoidDBusMethodCallback callback, dbus::Response* response) {
    std::move(callback).Run(response);
  }

  void OnUint64Method(DBusMethodCallback<uint64_t> callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    uint64_t result;
    if (!reader.PopUint64(&result)) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(result));
  }

  // Called when D-Bus method call which returns a string is completed or on
  // its error.
  void OnStringMethod(DBusMethodCallback<std::string> callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(result));
  }

  void OnEnableDebuggingFeatures(EnableDebuggingCallback callback,
                                 dbus::Response* response) {
    if (callback.is_null())
      return;

    std::move(callback).Run(response != nullptr);
  }

  void OnQueryDebuggingFeatures(QueryDevFeaturesCallback callback,
                                dbus::Response* response) {
    if (callback.is_null())
      return;

    int32_t feature_mask = DEV_FEATURE_NONE;
    if (!response || !dbus::MessageReader(response).PopInt32(&feature_mask)) {
      std::move(callback).Run(false,
                              debugd::DevFeatureFlag::DEV_FEATURES_DISABLED);
      return;
    }

    std::move(callback).Run(true, feature_mask);
  }

  void OnRemoveRootfsVerification(EnableDebuggingCallback callback,
                                  dbus::Response* response) {
    if (callback.is_null())
      return;

    std::move(callback).Run(response != nullptr);
  }

  // Called when a response for StopAgentTracing() is received.
  void OnStopAgentTracing(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request systrace stop";
      // If debugd crashes or completes I/O before this message is processed
      // then pipe_reader_ can be NULL, see OnIOComplete().
      if (pipe_reader_.get()) {
        pipe_reader_.reset();
        std::move(callback_).Run(GetTracingAgentName(), GetTraceEventLabel(),
                                 scoped_refptr<base::RefCountedString>(
                                     new base::RefCountedString()));
      }
    }
    // NB: requester is signaled when i/o completes
  }

  void OnTestICMP(TestICMPCallback callback, dbus::Response* response) {
    std::string status;
    if (!response || !dbus::MessageReader(response).PopString(&status)) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(status);
  }

  // Called when pipe i/o completes; pass data on and delete the instance.
  void OnIOComplete(base::Optional<std::string> result) {
    pipe_reader_.reset();
    std::string pipe_data =
        result.has_value() ? std::move(result).value() : std::string();
    std::move(callback_).Run(GetTracingAgentName(), GetTraceEventLabel(),
                             base::RefCountedString::TakeString(&pipe_data));
  }

  void OnSetOomScoreAdj(SetOomScoreAdjCallback callback,
                        dbus::Response* response) {
    std::string output;
    if (response && dbus::MessageReader(response).PopString(&output))
      std::move(callback).Run(true, output);
    else
      std::move(callback).Run(false, "");
  }

  void OnPrinterAdded(CupsAddPrinterCallback callback,
                      dbus::Response* response,
                      dbus::ErrorResponse* err_response) {
    int32_t result;

    // If we get a normal response, we need not examine the error response.
    if (response && dbus::MessageReader(response).PopInt32(&result)) {
      DCHECK_GE(result, 0);
      std::move(callback).Run(result);
      return;
    }

    // Without a normal response, we communicate the D-Bus error response
    // to the callback.
    std::string err_str;
    if (err_response) {
      dbus::MessageReader err_reader(err_response);
      err_str = err_response->GetErrorName();
    }
    DbusLibraryError dbus_error = DbusLibraryErrorFromString(err_str);
    std::move(callback).Run(dbus_error);
  }

  void OnPrinterRemoved(CupsRemovePrinterCallback callback,
                        base::OnceClosure error_callback,
                        dbus::Response* response) {
    bool result = false;
    if (response && dbus::MessageReader(response).PopBool(&result))
      std::move(callback).Run(result);
    else
      std::move(error_callback).Run();
  }

  void OnStartConcierge(ConciergeCallback callback, dbus::Response* response) {
    bool result = false;
    if (response) {
      dbus::MessageReader reader(response);
      reader.PopBool(&result);
    }
    std::move(callback).Run(result);
  }

  void OnStopConcierge(ConciergeCallback callback, dbus::Response* response) {
    // Debugd just sends back an empty response, so we just check if
    // the response exists
    std::move(callback).Run(response != nullptr);
  }

  void OnStartPluginVmDispatcher(PluginVmDispatcherCallback callback,
                                 std::string owner_id,
                                 dbus::Response* response,
                                 dbus::ErrorResponse* error) {
    if (error) {
      // Older versions of Chrome OS do not handle the |lang| arg, call again
      // with just |owner_id| for now.
      // TODO(crbug.com/1072082): Remove once new CrOS code is in Beta.
      if (error->GetErrorName() == DBUS_ERROR_INVALID_ARGS &&
          !owner_id.empty()) {
        LOG(ERROR) << "Failed to start dispatcher due to invalid arguments in "
                      "DBus call, retrying without language argument";
        dbus::MethodCall method_call(debugd::kDebugdInterface,
                                     debugd::kStartVmPluginDispatcher);
        dbus::MessageWriter writer(&method_call);
        writer.AppendString(owner_id);
        debugdaemon_proxy_->CallMethodWithErrorResponse(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
            base::BindOnce(&DebugDaemonClientImpl::OnStartPluginVmDispatcher,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::string()));
        return;
      }
      LOG(ERROR) << "Failed to start dispatcher, DBus error "
                 << error->GetErrorName();
      std::move(callback).Run(false);
      return;
    }

    bool result = false;
    if (response) {
      dbus::MessageReader reader(response);
      reader.PopBool(&result);
    }
    std::move(callback).Run(result);
  }

  void OnStopPluginVmDispatcher(PluginVmDispatcherCallback callback,
                                dbus::Response* response) {
    // Debugd just sends back an empty response, so we just check if
    // the response exists
    std::move(callback).Run(response != nullptr);
  }

  void OnSetRlzPingSent(SetRlzPingSentCallback callback,
                        dbus::Response* response) {
    bool result = false;
    if (response) {
      dbus::MessageReader reader(response);
      reader.PopBool(&result);
    }
    std::move(callback).Run(result);
  }

  void OnSetKstaledRatio(KstaledRatioCallback callback,
                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to read debugd response";
      std::move(callback).Run(false);
      return;
    }

    bool result = false;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&result)) {
      LOG(ERROR) << "Debugd response did not contain a bool";
      std::move(callback).Run(false);
      return;
    }

    std::move(callback).Run(result);
  }

  void OnSetSchedulerConfigurationV2(
      SetSchedulerConfigurationV2Callback callback,
      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(false, 0);
      return;
    }

    bool result = false;
    uint32_t num_cores_disabled = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&result) || !reader.PopUint32(&num_cores_disabled)) {
      LOG(ERROR) << "Failed to read SetSchedulerConfigurationV2 response";
      std::move(callback).Run(false, 0);
      return;
    }

    std::move(callback).Run(result, num_cores_disabled);
  }

  void OnGetU2fFlags(DBusMethodCallback<std::set<std::string>> callback,
                     dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::string flags_string;
    dbus::MessageReader reader(response);
    if (!reader.PopString(&flags_string)) {
      LOG(ERROR) << "Failed to read GetU2fFlags response";
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::set<std::string> flags;
    for (const auto& flag :
         base::SplitString(flags_string, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      flags.insert(flag);
    }

    std::move(callback).Run(std::move(flags));
  }

  dbus::ObjectProxy* debugdaemon_proxy_;
  std::unique_ptr<PipeReader> pipe_reader_;
  StopAgentTracingCallback callback_;
  scoped_refptr<base::TaskRunner> stop_agent_tracing_task_runner_;
  base::WeakPtrFactory<DebugDaemonClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DebugDaemonClientImpl);
};

DebugDaemonClient::DebugDaemonClient() = default;

DebugDaemonClient::~DebugDaemonClient() = default;

// static
std::unique_ptr<DebugDaemonClient> DebugDaemonClient::Create() {
  return std::make_unique<DebugDaemonClientImpl>();
}

}  // namespace chromeos
