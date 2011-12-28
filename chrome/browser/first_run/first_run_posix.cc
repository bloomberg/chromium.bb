// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"

namespace first_run {

namespace internal {

bool GetFirstRunSentinelFilePath(FilePath* path) {
  FilePath first_run_sentinel;

  if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
    return false;

  *path = first_run_sentinel.AppendASCII(kSentinelFile);
  return true;
}

}  // namespace internal

}  // namespace first_run
