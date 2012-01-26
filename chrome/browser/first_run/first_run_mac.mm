// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/mac/master_prefs.h"
#include "chrome/browser/process_singleton.h"

namespace first_run {
namespace internal {

bool ImportBookmarks(const FilePath& import_bookmarks_path) {
  // http://crbug.com/48880
  return false;
}

}  // namespace internal
}  // namespace first_run

namespace first_run {

void AutoImport(
    Profile* profile,
    bool homepage_defined,
    int import_items,
    int dont_import_items,
    bool make_chrome_default,
    ProcessSingleton* process_singleton) {
  // We need to avoid dispatching new tabs when we are importing because
  // that will lead to data corruption or a crash. Because there is no UI for
  // the import process, we pass NULL as the window to bring to the foreground
  // when a CopyData message comes in; this causes the message to be silently
  // discarded, which is the correct behavior during the import process.
  process_singleton->Lock(NULL);

  scoped_refptr<ImporterHost> importer_host;
  // TODO(csilv,mirandac): Out-of-process import has only been qualified on
  // MacOS X, so we will only use it on that platform since it is required.
  // Remove this conditional logic once oop import is qualified for
  // Linux/Windows. http://crbug.com/22142
  importer_host = new ExternalProcessImporterHost;

  internal::AutoImportPlatformCommon(importer_host,
                                     profile,
                                     homepage_defined,
                                     import_items,
                                     dont_import_items,
                                     make_chrome_default);

  process_singleton->Unlock();
  first_run::CreateSentinel();
}

FilePath MasterPrefsPath() {
  return master_prefs::MasterPrefsPath();
}

}  //namespace first_run
