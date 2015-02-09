/* -*- c++ -*- */
/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_SERVICE_RUNTIME_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_SERVICE_RUNTIME_H_

#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/public/imc_types.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class ErrorInfo;
class Plugin;
class SelLdrLauncherChrome;
class SrpcClient;
class ServiceRuntime;

// Struct of params used by StartSelLdr.  Use a struct so that callback
// creation templates aren't overwhelmed with too many parameters.
struct SelLdrStartParams {
  SelLdrStartParams(const std::string& url,
                    const PP_NaClFileInfo& file_info,
                    PP_NaClAppProcessType process_type)
      : url(url),
        file_info(file_info),
        process_type(process_type) {
  }
  std::string url;
  PP_NaClFileInfo file_info;
  PP_NaClAppProcessType process_type;
};

//  ServiceRuntime abstracts a NativeClient sel_ldr instance.
class ServiceRuntime {
 public:
  ServiceRuntime(Plugin* plugin,
                 PP_Instance pp_instance,
                 bool main_service_runtime,
                 bool uses_nonsfi_mode);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Spawn the sel_ldr instance.
  void StartSelLdr(const SelLdrStartParams& params,
                   pp::CompletionCallback callback);

  // If starting sel_ldr from a background thread, wait for sel_ldr to
  // actually start. Returns |false| if timed out waiting for the process
  // to start. Otherwise, returns |true| if StartSelLdr is complete
  // (either successfully or unsuccessfully).
  bool WaitForSelLdrStart();

  // Signal to waiting threads that StartSelLdr is complete (either
  // successfully or unsuccessfully).
  void SignalStartSelLdrDone();

  // If starting the nexe from a background thread, wait for the nexe to
  // actually start. Returns |true| is the nexe started successfully.
  bool WaitForNexeStart();

  // Returns |true| if WaitForSelLdrStart() timed out.
  bool SelLdrWaitTimedOut();

  // Signal to waiting threads that LoadNexeAndStart is complete (either
  // successfully or unsuccessfully).
  void SignalNexeStarted(bool ok);

  // Establish an SrpcClient to the sel_ldr instance and start the nexe.
  // This function must be called on the main thread.
  // This function must only be called once.
  void StartNexe();

  // Starts the application channel to the nexe.
  SrpcClient* SetupAppChannel();

  bool RemoteLog(int severity, const std::string& msg);
  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  bool main_service_runtime() const { return main_service_runtime_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);
  bool StartNexeInternal();

  bool SetupCommandChannel();
  bool StartModule();
  void ReapLogs();

  void ReportLoadError(const ErrorInfo& error_info);

  NaClSrpcChannel command_channel_;
  Plugin* plugin_;
  PP_Instance pp_instance_;
  bool main_service_runtime_;
  bool uses_nonsfi_mode_;
  nacl::scoped_ptr<SelLdrLauncherChrome> subprocess_;

  // Mutex and CondVar to protect start_sel_ldr_done_ and nexe_started_.
  NaClMutex mu_;
  NaClCondVar cond_;
  bool start_sel_ldr_done_;
  bool sel_ldr_wait_timed_out_;
  bool start_nexe_done_;
  bool nexe_started_ok_;

  NaClHandle bootstrap_channel_;
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_SERVICE_RUNTIME_H_
