// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/pepper_plugin_info.h"

namespace content {

PepperPluginInfo::PepperPluginInfo()
    : is_internal(false),
      is_out_of_process(false),
      is_sandboxed(true) {
}

PepperPluginInfo::~PepperPluginInfo() {
}

}  // namespace content
