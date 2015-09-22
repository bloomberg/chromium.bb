// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_util.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

bool MdDownloadsEnabled() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kEnableMaterialDesignDownloads))
    return true;

  if (cl->HasSwitch(switches::kDisableMaterialDesignDownloads))
    return false;

  return true;
}
