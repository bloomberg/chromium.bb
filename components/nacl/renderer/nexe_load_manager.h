// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_NEXE_LOAD_MANAGER_H_
#define COMPONENTS_NACL_RENDERER_NEXE_LOAD_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

#include "ppapi/c/private/ppb_nacl_private.h"

namespace content {
class PepperPluginInstance;
}

namespace nacl {

class TrustedPluginChannel;

// NexeLoadManager provides methods for reporting the progress of loading a
// nexe.
class NexeLoadManager {
 public:
  explicit NexeLoadManager(PP_Instance instance);
  ~NexeLoadManager();

  void ReportLoadError(PP_NaClError error,
                       const std::string& error_message,
                       const std::string& console_message);

  // TODO(dmichael): Everything below this comment should eventually be made
  // private, when ppb_nacl_private_impl.cc is no longer using them directly.
  // The intent is for this class to only expose functions for reporting a
  // load state transition (e.g., ReportLoadError, ReportProgress,
  // ReportLoadAbort, etc.)
  struct ProgressEvent {
    explicit ProgressEvent(PP_NaClEventType event_type_param)
        : event_type(event_type_param),
          length_is_computable(false),
          loaded_bytes(0),
          total_bytes(0) {
    }
    PP_Instance instance;
    PP_NaClEventType event_type;
    std::string resource_url;
    bool length_is_computable;
    uint64_t loaded_bytes;
    uint64_t total_bytes;
  };
  void DispatchEvent(const ProgressEvent &event);
  void set_trusted_plugin_channel(scoped_ptr<TrustedPluginChannel> channel);

  bool nexe_error_reported();
  void set_nexe_error_reported(bool error_reported);

  PP_NaClReadyState nacl_ready_state();
  void set_nacl_ready_state(PP_NaClReadyState ready_state);

  void SetReadOnlyProperty(PP_Var key, PP_Var value);
  void LogToConsole(const std::string& message);

  bool is_installed() { return is_installed_; }
  void set_is_installed(bool installed) { is_installed_ = installed; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NexeLoadManager);

  PP_Instance pp_instance_;
  PP_NaClReadyState nacl_ready_state_;
  bool nexe_error_reported_;

  // A flag indicating if the NaCl executable is being loaded from an installed
  // application.  This flag is used to bucket UMA statistics more precisely to
  // help determine whether nexe loading problems are caused by networking
  // issues.  (Installed applications will be loaded from disk.)
  // Unfortunately, the definition of what it means to be part of an installed
  // application is a little murky - for example an installed application can
  // register a mime handler that loads NaCl executables into an arbitrary web
  // page.  As such, the flag actually means "our best guess, based on the URLs
  // for NaCl resources that we have seen so far".
  bool is_installed_;

  // Non-owning.
  content::PepperPluginInstance* plugin_instance_;

  scoped_ptr<TrustedPluginChannel> trusted_plugin_channel_;
  base::WeakPtrFactory<NexeLoadManager> weak_factory_;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_NEXE_LOAD_MANAGER_H_
