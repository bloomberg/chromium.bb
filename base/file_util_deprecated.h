// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#include "build/build_config.h"

// We've successfully deprecated all of these functions on non-Windows
// platforms.

#if defined(OS_WIN)

namespace file_util {

// Use the FilePath versions instead.
FILE* OpenFile(const std::string& filename, const char* mode);
FILE* OpenFile(const std::wstring& filename, const char* mode);

// Use FilePath::DirName instead.
void UpOneDirectory(std::wstring* dir);

// Use FilePath::BaseName instead.
std::wstring GetFilenameFromPath(const std::wstring& path);

// Returns the directory component of a path, without the trailing
// path separator, or an empty string on error. The function does not
// check for the existence of the path, so if it is passed a directory
// without the trailing \, it will interpret the last component of the
// path as a file and chomp it. This does not support relative paths.
// Examples:
// path == "C:\pics\jojo.jpg",     returns "C:\pics"
// path == "C:\Windows\system32\", returns "C:\Windows\system32"
// path == "C:\Windows\system32",  returns "C:\Windows"
// Deprecated. Use FilePath's DirName() instead.
std::wstring GetDirectoryFromPath(const std::wstring& path);

// Appends new_ending to path, adding a separator between the two if necessary.
void AppendToPath(std::wstring* path, const std::wstring& new_ending);

// Use FilePath::Extension instead.
FilePath::StringType GetFileExtensionFromPath(const FilePath& path);
std::wstring GetFileExtensionFromPath(const std::wstring& path);

bool AbsolutePath(std::wstring* path);

// Use FilePath::InsertBeforeExtension.
void InsertBeforeExtension(FilePath* path, const FilePath::StringType& suffix);

// Use version that takes a FilePath.
bool Delete(const std::wstring& path, bool recursive);
bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive);
bool ReadFileToString(const std::wstring& path, std::string* contents);
int ReadFile(const std::wstring& filename, char* data, int size);
int WriteFile(const std::wstring& filename, const char* data, int size);

}

#endif  // OS_WIN

#endif  // BASE_FILE_UTIL_DEPRECATED_H_
