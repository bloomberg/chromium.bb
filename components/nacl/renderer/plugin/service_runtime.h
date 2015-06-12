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

  // Establish an SrpcClient to the sel_ldr instance and start the nexe.
  // This function must be called on the main thread.
  // This function must only be called once.
  void StartNexe();

  // Starts the application channel to the nexe.
  SrpcClient* SetupAppChannel();

  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  bool main_service_runtime() const { return main_service_runtime_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);

  bool SetupCommandChannel();

  void ReportLoadError(const ErrorInfo& error_info);

  Plugin* plugin_;
  PP_Instance pp_instance_;
  bool main_service_runtime_;
  bool uses_nonsfi_mode_;
  nacl::scoped_ptr<SelLdrLauncherChrome> subprocess_;

  NaClHandle bootstrap_channel_;
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_SERVICE_RUNTIME_H_
