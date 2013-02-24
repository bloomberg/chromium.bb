// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/files/file_path.h"
#include "base/string_util.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/mac/master_prefs.h"
#include "chrome/browser/process_singleton.h"

namespace first_run {
namespace internal {

bool ImportBookmarks(const base::FilePath& import_bookmarks_path) {
  // http://crbug.com/48880
  return false;
}

}  // namespace internal
}  // namespace first_run

namespace first_run {

base::FilePath MasterPrefsPath() {
  return master_prefs::MasterPrefsPath();
}

}  //namespace first_run
