// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"

const char kMaterialDesignDownloadsFinchTrialName[] = "MaterialDesignDownloads";

bool MdDownloadsEnabled() {
  // Intentionally call this before checking command line to assign a group.
  std::string group = base::FieldTrialList::FindFullName(
      kMaterialDesignDownloadsFinchTrialName);

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kEnableMaterialDesignDownloads))
    return true;

  if (cl->HasSwitch(switches::kDisableMaterialDesignDownloads))
    return false;

  return base::StartsWith(group, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}
