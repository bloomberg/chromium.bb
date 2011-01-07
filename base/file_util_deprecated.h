// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We're trying to transition away from paths as wstrings into using
// FilePath objects.  This file contains declarations of deprecated
// functions.  By hiding them here rather in the main header, we hope
// to discourage callers.

// See file_util.h for documentation on all functions that don't have
// documentation here.

#ifndef BASE_FILE_UTIL_DEPRECATED_H_
#define BASE_FILE_UTIL_DEPRECATED_H_
#pragma once

#include "build/build_config.h"

// We've successfully deprecated all of these functions on non-Windows
// platforms.

#if defined(OS_WIN)

namespace file_util {

// Use the FilePath versions instead.
FILE* OpenFile(const std::string& filename, const char* mode);
FILE* OpenFile(const std::wstring& filename, const char* mode);

// Appends new_ending to path, adding a separator between the two if necessary.
void AppendToPath(std::wstring* path, const std::wstring& new_ending);

// Use FilePath::Extension instead.
FilePath::StringType GetFileExtensionFromPath(const FilePath& path);
std::wstring GetFileExtensionFromPath(const std::wstring& path);

// Use version that takes a FilePath.
bool Delete(const std::wstring& path, bool recursive);
bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive);
int ReadFile(const std::wstring& filename, char* data, int size);
int WriteFile(const std::wstring& filename, const char* data, int size);

}

#endif  // OS_WIN

#endif  // BASE_FILE_UTIL_DEPRECATED_H_
