// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

class CommandLine;
class FilePath;
class GURL;
class ImporterHost;
class ImporterList;
class Profile;
class ProcessSingleton;
class TemplateURLService;

namespace first_run {
namespace internal {

enum FirstRunState {
  FIRST_RUN_UNKNOWN,  // The state is not tested or set yet.
  FIRST_RUN_TRUE,
  FIRST_RUN_FALSE
};

// This variable should only be accessed through IsChromeFirstRun().
extern FirstRunState first_run_;

// The kSentinelFile file absence will tell us it is a first run.
extern const char* const kSentinelFile;

// -- Platform-specific functions --

// Gives the full path to the sentinel file. The file might not exist.
// This function has a common implementation on OS_POSIX and a windows specific
// implementation.
bool GetFirstRunSentinelFilePath(FilePath* path);

// This function has a common implementationin for all non-linux platforms, and
// a linux specific implementation.
bool IsOrganicFirstRun();

// Imports settings. This may be done in a separate process depending on the
// platform, but it will always block until done. The return value indicates
// success.
// This functions has a common implementation for OS_POSIX, and a
// windows specific implementation.
bool ImportSettings(Profile* profile,
                    scoped_refptr<ImporterHost> importer_host,
                    scoped_refptr<ImporterList> importer_list,
                    int items_to_import);

#if defined(OS_WIN)
// TODO(jennyz): This fuction will be moved to first_run_win.cc anonymous
// namespace once we refactor its calling code in first_run.cc later.
bool ImportSettingsWin(Profile* profile,
                       int importer_type,
                       int items_to_import,
                       const FilePath& import_bookmarks_path,
                       bool skip_first_run_ui);
#endif  // OS_WIN

#if !defined(USE_AURA)
// AutoImport code which is common to all platforms.
void AutoImportPlatformCommon(
    scoped_refptr<ImporterHost> importer_host,
    Profile* profile,
    bool homepage_defined,
    int import_items,
    int dont_import_items,
    bool search_engine_experiment,
    bool randomize_search_engine_experiment,
    bool make_chrome_default);
#endif  // !defined(USE_AURA)

int ImportBookmarkFromFileIfNeeded(Profile* profile,
                                   const CommandLine& cmdline);

#if !defined(OS_WIN)
bool ImportBookmarks(const FilePath& import_bookmarks_path);
#endif

}  // namespace internal
}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
