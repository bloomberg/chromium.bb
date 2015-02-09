/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NACL_LOG_MODULE_NAME "Plugin_ServiceRuntime"

#include "components/nacl/renderer/plugin/service_runtime.h"

#include <string.h>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/plugin_error.h"
#include "components/nacl/renderer/plugin/pnacl_resources.h"
#include "components/nacl/renderer/plugin/sel_ldr_launcher_chrome.h"
#include "components/nacl/renderer/plugin/srpc_client.h"
#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/public/imc_types.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"

namespace plugin {

ServiceRuntime::ServiceRuntime(Plugin* plugin,
                               PP_Instance pp_instance,
                               bool main_service_runtime,
                               bool uses_nonsfi_mode)
    : plugin_(plugin),
      pp_instance_(pp_instance),
      main_service_runtime_(main_service_runtime),
      uses_nonsfi_mode_(uses_nonsfi_mode),
      start_sel_ldr_done_(false),
      sel_ldr_wait_timed_out_(false),
      start_nexe_done_(false),
      nexe_started_ok_(false),
      bootstrap_channel_(NACL_INVALID_HANDLE) {
  NaClSrpcChannelInitialize(&command_channel_);
  NaClXMutexCtor(&mu_);
  NaClXCondVarCtor(&cond_);
}

bool ServiceRuntime::SetupCommandChannel() {
  NaClLog(4, "ServiceRuntime::SetupCommand (this=%p, subprocess=%p)\n",
          static_cast<void*>(this),
          static_cast<void*>(subprocess_.get()));
  // Set up the bootstrap channel in our subprocess so that we can establish
  // SRPC.
  subprocess_->set_channel(bootstrap_channel_);

  if (uses_nonsfi_mode_) {
    // In non-SFI mode, no SRPC is used. Just skips and returns success.
    return true;
  }

  if (!subprocess_->SetupCommand(&command_channel_)) {
    ErrorInfo error_info;
    error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL,
                         "ServiceRuntime: command channel creation failed");
    ReportLoadError(error_info);
    return false;
  }
  return true;
}

bool ServiceRuntime::StartModule() {
  // start the module.  otherwise we cannot connect for multimedia
  // subsystem since that is handled by user-level code (not secure!)
  // in libsrpc.
  int load_status = -1;
  if (uses_nonsfi_mode_) {
    // In non-SFI mode, we don't need to call start_module SRPC to launch
    // the plugin.
    load_status = LOAD_OK;
  } else {
    NaClSrpcResultCodes rpc_result =
        NaClSrpcInvokeBySignature(&command_channel_,
                                  "start_module::i",
                                  &load_status);

    if (NACL_SRPC_RESULT_OK != rpc_result) {
      ErrorInfo error_info;
      error_info.SetReport(PP_NACL_ERROR_SEL_LDR_START_MODULE,
                           "ServiceRuntime: could not start nacl module");
      ReportLoadError(error_info);
      return false;
    }
  }

  NaClLog(4, "ServiceRuntime::StartModule (load_status=%d)\n", load_status);
  if (main_service_runtime_) {
    if (load_status < 0 || load_status > NACL_ERROR_CODE_MAX)
      load_status = LOAD_STATUS_UNKNOWN;
    GetNaClInterface()->ReportSelLdrStatus(pp_instance_,
                                           load_status,
                                           NACL_ERROR_CODE_MAX);
  }

  if (LOAD_OK != load_status) {
    ErrorInfo error_info;
    error_info.SetReport(
        PP_NACL_ERROR_SEL_LDR_START_STATUS,
        NaClErrorString(static_cast<NaClErrorCode>(load_status)));
    ReportLoadError(error_info);
    return false;
  }
  return true;
}

void ServiceRuntime::StartSelLdr(const SelLdrStartParams& params,
                                 pp::CompletionCallback callback) {
  NaClLog(4, "ServiceRuntime::Start\n");

  nacl::scoped_ptr<SelLdrLauncherChrome>
      tmp_subprocess(new SelLdrLauncherChrome());
  if (NULL == tmp_subprocess.get()) {
    NaClLog(LOG_ERROR, "ServiceRuntime::Start (subprocess create failed)\n");
    ErrorInfo error_info;
    error_info.SetReport(
        PP_NACL_ERROR_SEL_LDR_CREATE_LAUNCHER,
        "ServiceRuntime: failed to create sel_ldr launcher");
    ReportLoadError(error_info);
    pp::Module::Get()->core()->CallOnMainThread(0, callback, PP_ERROR_FAILED);
    return;
  }

  GetNaClInterface()->LaunchSelLdr(
      pp_instance_,
      PP_FromBool(main_service_runtime_),
      params.url.c_str(),
      &params.file_info,
      PP_FromBool(uses_nonsfi_mode_),
      params.process_type,
      &bootstrap_channel_,
      callback.pp_completion_callback());
  subprocess_.reset(tmp_subprocess.release());
}

bool ServiceRuntime::WaitForSelLdrStart() {
  // Time to wait on condvar (for browser to create a new sel_ldr process on
  // our behalf). Use 6 seconds to be *fairly* conservative.
  //
  // On surfaway, the CallOnMainThread above may never get scheduled
  // to unblock this condvar, or the IPC reply from the browser to renderer
  // might get canceled/dropped. However, it is currently important to
  // avoid waiting indefinitely because ~PnaclCoordinator will attempt to
  // join() the PnaclTranslateThread, and the PnaclTranslateThread is waiting
  // for the signal before exiting.
  static int64_t const kWaitTimeMicrosecs = 6 * NACL_MICROS_PER_UNIT;
  int64_t left_to_wait = kWaitTimeMicrosecs;
  int64_t deadline = NaClGetTimeOfDayMicroseconds() + left_to_wait;
  nacl::MutexLocker take(&mu_);
  while(!start_sel_ldr_done_ && left_to_wait > 0) {
    struct nacl_abi_timespec left_timespec;
    left_timespec.tv_sec = left_to_wait / NACL_MICROS_PER_UNIT;
    left_timespec.tv_nsec =
        (left_to_wait % NACL_MICROS_PER_UNIT) * NACL_NANOS_PER_MICRO;
    NaClXCondVarTimedWaitRelative(&cond_, &mu_, &left_timespec);
    int64_t now = NaClGetTimeOfDayMicroseconds();
    left_to_wait = deadline - now;
  }
  if (left_to_wait <= 0)
    sel_ldr_wait_timed_out_ = true;
  return start_sel_ldr_done_;
}

void ServiceRuntime::SignalStartSelLdrDone() {
  nacl::MutexLocker take(&mu_);
  start_sel_ldr_done_ = true;
  NaClXCondVarSignal(&cond_);
}

bool ServiceRuntime::SelLdrWaitTimedOut() {
  nacl::MutexLocker take(&mu_);
  return sel_ldr_wait_timed_out_;
}

bool ServiceRuntime::WaitForNexeStart() {
  nacl::MutexLocker take(&mu_);
  while (!start_nexe_done_)
    NaClXCondVarWait(&cond_, &mu_);
  return nexe_started_ok_;
}

void ServiceRuntime::SignalNexeStarted(bool ok) {
  nacl::MutexLocker take(&mu_);
  start_nexe_done_ = true;
  nexe_started_ok_ = ok;
  NaClXCondVarSignal(&cond_);
}

void ServiceRuntime::StartNexe() {
  bool ok = StartNexeInternal();
  if (ok) {
    NaClLog(4, "ServiceRuntime::StartNexe (success)\n");
  } else {
    ReapLogs();
  }
  // This only matters if a background thread is waiting, but we signal in all
  // cases to simplify the code.
  SignalNexeStarted(ok);
}

bool ServiceRuntime::StartNexeInternal() {
  if (!SetupCommandChannel())
    return false;
  return StartModule();
}

void ServiceRuntime::ReapLogs() {
  // TODO(teravest): We should allow the NaCl process to crash itself when a
  // module fails to start, and remove the call to RemoteLog() here. The
  // reverse channel is no longer needed for crash reporting.
  //
  // The reasoning behind the current code behavior follows:
  // On a load failure the NaCl process does not crash itself to
  // avoid a race where the no-more-senders error on the reverse
  // channel service thread might cause the crash-detection logic to
  // kick in before the start_module RPC reply has been received. So
  // we induce a NaCl process crash here.
  RemoteLog(LOG_FATAL, "reap logs\n");

  // TODO(teravest): Release subprocess_ here since it's no longer needed. It
  // was previously kept around to collect crash log output from the bootstrap
  // channel.
}

void ServiceRuntime::ReportLoadError(const ErrorInfo& error_info) {
  if (main_service_runtime_) {
    plugin_->ReportLoadError(error_info);
  }
}

SrpcClient* ServiceRuntime::SetupAppChannel() {
  NaClLog(4, "ServiceRuntime::SetupAppChannel (subprocess_=%p)\n",
          reinterpret_cast<void*>(subprocess_.get()));
  nacl::DescWrapper* connect_desc = subprocess_->socket_addr()->Connect();
  if (NULL == connect_desc) {
    NaClLog(LOG_ERROR, "ServiceRuntime::SetupAppChannel (connect failed)\n");
    return NULL;
  } else {
    NaClLog(4, "ServiceRuntime::SetupAppChannel (conect_desc=%p)\n",
            static_cast<void*>(connect_desc));
    SrpcClient* srpc_client = SrpcClient::New(connect_desc);
    NaClLog(4, "ServiceRuntime::SetupAppChannel (srpc_client=%p)\n",
            static_cast<void*>(srpc_client));
    delete connect_desc;
    return srpc_client;
  }
}

bool ServiceRuntime::RemoteLog(int severity, const std::string& msg) {
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "log:is:",
                                severity,
                                strdup(msg.c_str()));
  return (NACL_SRPC_RESULT_OK == rpc_result);
}

void ServiceRuntime::Shutdown() {
  // Abandon callbacks, tell service threads to quit if they were
  // blocked waiting for main thread operations to finish.  Note that
  // some callbacks must still await their completion event, e.g.,
  // CallOnMainThread must still wait for the time out, or I/O events
  // must finish, so resources associated with pending events cannot
  // be deallocated.

  // Note that this does waitpid() to get rid of any zombie subprocess.
  subprocess_.reset(NULL);

  NaClSrpcDtor(&command_channel_);
}

ServiceRuntime::~ServiceRuntime() {
  NaClLog(4, "ServiceRuntime::~ServiceRuntime (this=%p)\n",
          static_cast<void*>(this));
  // We do this just in case Shutdown() was not called.
  subprocess_.reset(NULL);

  NaClCondVarDtor(&cond_);
  NaClMutexDtor(&mu_);
}

}  // namespace plugin
