/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "components/nacl/renderer/plugin/service_runtime.h"

#include <string.h>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/plugin_error.h"
#include "components/nacl/renderer/plugin/sel_ldr_launcher_chrome.h"
#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/public/imc_types.h"
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
      bootstrap_channel_(NACL_INVALID_HANDLE),
      process_id_(base::kNullProcessId) {
}

void ServiceRuntime::StartSelLdr(const SelLdrStartParams& params,
                                 pp::CompletionCallback callback) {
  nacl::scoped_ptr<SelLdrLauncherChrome>
      tmp_subprocess(new SelLdrLauncherChrome());
  if (NULL == tmp_subprocess.get()) {
    LOG(ERROR) << "ServiceRuntime::Start (subprocess create failed)";
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
      &translator_channel_,
      &process_id_,
      callback.pp_completion_callback());
  subprocess_.reset(tmp_subprocess.release());
}

void ServiceRuntime::ReportLoadError(const ErrorInfo& error_info) {
  if (main_service_runtime_) {
    plugin_->ReportLoadError(error_info);
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
  // We do this just in case Shutdown() was not called.
  subprocess_.reset(NULL);
}

}  // namespace plugin
