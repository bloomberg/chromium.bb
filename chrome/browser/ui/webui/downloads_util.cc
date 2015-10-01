// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_util.h"

#include "base/command_line.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"

bool MdDownloadsEnabled() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kEnableMaterialDesignDownloads))
    return true;

  if (cl->HasSwitch(switches::kDisableMaterialDesignDownloads))
    return false;

  switch (chrome::GetChannel()) {
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
    case version_info::Channel::UNKNOWN:
      return true;
    default:
      return false;
  }
}
