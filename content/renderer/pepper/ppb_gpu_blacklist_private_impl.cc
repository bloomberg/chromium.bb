// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_gpu_blacklist_private_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"

// todo(nfullagar): Remove this private interface when the SRPC proxy is
// permanently disabled.

namespace content {

namespace {

PP_Bool IsGpuBlacklisted() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line)
    return PP_FromBool(
        command_line->HasSwitch(switches::kDisablePepper3d));
  return PP_TRUE;
}

}  // namespace

const PPB_GpuBlacklist_Private ppb_gpu_blacklist = {
  &IsGpuBlacklisted,
};

// static
const PPB_GpuBlacklist_Private* PPB_GpuBlacklist_Private_Impl::GetInterface() {
  return &ppb_gpu_blacklist;
}

}  // namespace content

