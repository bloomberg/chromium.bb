// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Instances of NaCl modules spun up within the plugin as a subprocess.
// This may represent the "main" nacl module, or it may represent helpers
// that perform various tasks within the plugin, for example,
// a NaCl module for a compiler could be loaded to translate LLVM bitcode
// into native code.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_NACL_SUBPROCESS_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_NACL_SUBPROCESS_H_

#include <stdarg.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/nacl/renderer/plugin/service_runtime.h"
#include "native_client/src/include/portability.h"

namespace plugin {

class Plugin;
class ServiceRuntime;


// A class representing an instance of a NaCl module, loaded by the plugin.
class NaClSubprocess {
 public:
  NaClSubprocess(const std::string& description,
                 ServiceRuntime* service_runtime);
  virtual ~NaClSubprocess();

  ServiceRuntime* service_runtime() const { return service_runtime_.get(); }
  void set_service_runtime(ServiceRuntime* service_runtime) {
    service_runtime_.reset(service_runtime);
  }

  // A basic description of the subprocess.
  std::string description() const { return description_; }

  // A detailed description of the subprocess that may contain addresses.
  // Only use for debugging, but do not expose this to untrusted webapps.
  std::string detailed_description() const;

  // Fully shut down the subprocess.
  void Shutdown();

 private:
  std::string description_;

  // The service runtime representing the NaCl module instance.
  scoped_ptr<ServiceRuntime> service_runtime_;

  DISALLOW_COPY_AND_ASSIGN(NaClSubprocess);
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_NACL_SUBPROCESS_H_
