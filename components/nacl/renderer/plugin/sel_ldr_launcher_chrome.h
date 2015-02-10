// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_COMPONENTS_NACL_RENDERER_PLUGIN_SEL_LDR_LAUNCHER_CHROME_H_
#define PPAPI_COMPONENTS_NACL_RENDERER_PLUGIN_SEL_LDR_LAUNCHER_CHROME_H_

#include "components/nacl/renderer/ppb_nacl_private.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class SelLdrLauncherChrome : public nacl::SelLdrLauncherBase {
 public:
  bool Start(const char* url) override;

  // Provides a way for LaunchSelLdr() to write bootstrap channel information
  // into this class.
  void set_channel(NaClHandle channel);
};

}  // namespace plugin

#endif
