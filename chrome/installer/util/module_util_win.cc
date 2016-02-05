// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/module_util_win.h"

#include "base/base_paths.h"
#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"

namespace installer {

namespace {

// Returns the version in the current executable's version resource.
base::string16 GetCurrentExecutableVersion() {
  scoped_ptr<FileVersionInfo> file_version_info(
      CREATE_FILE_VERSION_INFO_FOR_CURRENT_MODULE());
  DCHECK(file_version_info.get());
  base::string16 version_string(file_version_info->file_version());
  DCHECK(base::Version(base::UTF16ToASCII(version_string)).IsValid());
  return version_string;
}

// Indicates whether a file can be opened using the same flags that
// ::LoadLibrary() uses to open modules.
bool ModuleCanBeRead(const base::FilePath file_path) {
  return base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ)
      .IsValid();
}

}  // namespace

base::FilePath GetModulePath(base::StringPiece16 module_name) {
  base::FilePath exe_dir;
  const bool has_path = base::PathService::Get(base::DIR_EXE, &exe_dir);
  DCHECK(has_path);

  // Look for the module in the current executable's directory and return the
  // path if it can be read. This is the expected location of modules for dev
  // builds.
  const base::FilePath module_path = exe_dir.Append(module_name);
  if (ModuleCanBeRead(module_path))
    return module_path;

  // Othwerwise, return the path to the module in a versioned sub-directory of
  // the current executable's directory. This is the expected location of
  // modules for proper installs.
  const base::string16 version = GetCurrentExecutableVersion();
  DCHECK(!version.empty());
  return exe_dir.Append(version).Append(module_name);
}

}  // namespace installer
