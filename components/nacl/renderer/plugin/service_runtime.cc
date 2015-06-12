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
      bootstrap_channel_(NACL_INVALID_HANDLE) {
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

  if (!subprocess_->ConnectBootstrapSocket()) {
    ErrorInfo error_info;
    error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL,
                         "ServiceRuntime: ConnectBootstrapSocket() failed");
    ReportLoadError(error_info);
    return false;
  }
  if (!subprocess_->RetrieveSockAddr()) {
    ErrorInfo error_info;
    error_info.SetReport(PP_NACL_ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL,
                         "ServiceRuntime: RetrieveSockAddr() failed");
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

void ServiceRuntime::StartNexe() {
  bool ok = SetupCommandChannel();
  if (ok) {
    NaClLog(4, "ServiceRuntime::StartNexe (success)\n");
  }
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
    NaClLog(4, "ServiceRuntime::SetupAppChannel (connect_desc=%p)\n",
            static_cast<void*>(connect_desc));
    SrpcClient* srpc_client = SrpcClient::New(connect_desc);
    NaClLog(4, "ServiceRuntime::SetupAppChannel (srpc_client=%p)\n",
            static_cast<void*>(srpc_client));
    delete connect_desc;
    return srpc_client;
  }
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
}

ServiceRuntime::~ServiceRuntime() {
  NaClLog(4, "ServiceRuntime::~ServiceRuntime (this=%p)\n",
          static_cast<void*>(this));
  // We do this just in case Shutdown() was not called.
  subprocess_.reset(NULL);
}

}  // namespace plugin
