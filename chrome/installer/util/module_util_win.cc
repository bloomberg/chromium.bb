// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/installer/util/module_util_win.h"

#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"

namespace installer {

namespace {

// Returns the directory in which the currently running executable resides.
base::FilePath GetExecutableDir() {
  base::char16 path[MAX_PATH];
  ::GetModuleFileNameW(nullptr, path, MAX_PATH);
  return base::FilePath(path).DirName();
}

// Returns the version in the current module's version resource or the empty
// string if none found.
base::string16 GetCurrentModuleVersion() {
  scoped_ptr<FileVersionInfo> file_version_info(
      CREATE_FILE_VERSION_INFO_FOR_CURRENT_MODULE());
  if (file_version_info.get()) {
    base::string16 version_string(file_version_info->file_version());
    if (Version(base::UTF16ToASCII(version_string)).IsValid())
      return version_string;
  }
  return base::string16();
}

// Indicates whether a file can be opened using the same flags that
// ::LoadLibrary() uses to open modules.
bool ModuleCanBeRead(const base::FilePath file_path) {
  return base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ)
      .IsValid();
}

}  // namespace

base::FilePath GetModulePath(base::StringPiece16 module_name,
                             base::string16* version) {
  DCHECK(version);

  base::FilePath module_dir = GetExecutableDir();
  base::FilePath module = module_dir.Append(module_name);
  if (ModuleCanBeRead(module))
    return module;

  base::string16 version_string(GetCurrentModuleVersion());
  if (version_string.empty()) {
    LOG(ERROR) << "No valid Chrome version found";
    return base::FilePath();
  }
  *version = version_string;
  return module_dir.Append(version_string).Append(module_name);
}

}  // namespace installer
