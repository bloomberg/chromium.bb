// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mash_util.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsRunningInMash() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(switches::kIsRunningInMash);
}

}  // namespace content
