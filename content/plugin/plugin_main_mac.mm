// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#include <servers/bootstrap.h>

#include "base/environment.h"
#include "base/mac/mach_logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "content/common/plugin_carbon_interpose_constants_mac.h"
#include "content/common/sandbox_init_mac.h"
#include "content/plugin/plugin_interpose_util_mac.h"
#include "content/public/common/content_client.h"

namespace content {

#if !defined(__LP64__)
void TrimInterposeEnvironment() {
  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::string interpose_list;
  if (!env->GetVar(kDYLDInsertLibrariesKey, &interpose_list)) {
    VLOG(0) << "No Carbon Interpose library found.";
    return;
  }

  // The list is a :-separated list of paths. Because we append our interpose
  // library just before forking in plugin_process_host.cc, the only cases we
  // need to handle are:
  // 1) The whole string is "<kInterposeLibraryPath>", so just clear it, or
  // 2) ":<kInterposeLibraryPath>" is the end of the string, so trim and re-set.
  std::string interpose_library_path =
      GetContentClient()->GetCarbonInterposePath();
  DCHECK_GE(interpose_list.size(), interpose_library_path.size());
  size_t suffix_offset = interpose_list.size() - interpose_library_path.size();
  if (suffix_offset == 0 &&
      interpose_list == interpose_library_path) {
    env->UnSetVar(kDYLDInsertLibrariesKey);
  } else if (suffix_offset > 0 && interpose_list[suffix_offset - 1] == ':' &&
             interpose_list.substr(suffix_offset) == interpose_library_path) {
    std::string trimmed_list = interpose_list.substr(0, suffix_offset - 1);
    env->SetVar(kDYLDInsertLibrariesKey, trimmed_list.c_str());
  } else {
    NOTREACHED() << "Missing Carbon interposing library";
  }
}
#endif

void InitializeChromeApplication() {
  // The bootstrap sandbox has taken over the bootstrap port. However, NPAPI
  // plugins request servers with the BOOTSTRAP_PER_PID_SERVICE flag. This
  // will fail, since the browser will be forwarding the message on behalf of
  // the plugin, and the browser has already created these per-pid services
  // for itself.
  //
  // Instead, request the real bootstrap port from the sandbox server, which
  // can then be used by the plugin.
  mach_port_t new_bootstrap_port = MACH_PORT_NULL;
  kern_return_t kr = bootstrap_look_up(bootstrap_port,
      kBootstrapPortNameForNPAPIPlugins, &new_bootstrap_port);
  BOOTSTRAP_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "Failed to look up original bootstrap port.";
  if (kr == KERN_SUCCESS) {
    bootstrap_port = new_bootstrap_port;
    kr = task_set_bootstrap_port(mach_task_self(), new_bootstrap_port);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
        << "Failed to reset TASK_BOOTSTRAP_PORT.";
  }


  [NSApplication sharedApplication];
  mac_plugin_interposing::SetUpCocoaInterposing();
}

}  // namespace content
