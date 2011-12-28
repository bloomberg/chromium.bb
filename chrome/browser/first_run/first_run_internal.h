// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

}  // namespace internal
}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
