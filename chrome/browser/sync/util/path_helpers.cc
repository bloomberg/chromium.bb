// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/path_helpers.h"

#include <Shlwapi.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/port.h"
#include "chrome/browser/sync/syncable/syncable.h"

#ifndef OS_WINDOWS
#error Compile this file on Windows only.
#endif

using std::string;

#if OS_WINDOWS
const char PATH_SEPARATOR = '\\';
#else
const char PATH_SEPARATOR = '/';
#endif  // OS_WINDOWS


static PathString RemoveTrailingSlashes16(PathString str) {
  while ((str.length() > 0) && (str.at(str.length() - 1) == kPathSeparator[0]))
    str.resize(str.length() - 1);
  return str;
}

static string RemoveTrailingSlashes(string str) {
  while ((str.length() > 0) && (str.at(str.length() - 1) == PATH_SEPARATOR))
    str.resize(str.length() - 1);
  return str;
}

PathString LastPathSegment(const PathString& path) {
  return RemoveTrailingSlashes16(PathFindFileNameW(path.c_str()));
}

string LastPathSegment(const string& path) {
  return RemoveTrailingSlashes(PathFindFileNameA(path.c_str()));
}

PathString GetFullPath(const PathString& path) {
  PathChar buffer[MAX_PATH];
  PathChar* file_part;
  DWORD const size = GetFullPathName(path.c_str(), ARRAYSIZE(buffer), buffer,
                                     &file_part);
  return PathString(buffer, size);
}

PathString AppendSlash(const PathString& path) {
  PathString result(path);
  if (!result.empty()) {
    if (*result.rbegin() == '/')
      *result.rbegin() = '\\';
    else if (*result.rbegin() != '\\')
      result.push_back('\\');
  }
  return result;
}

PathString ExpandTilde(const PathString& path) {
  // Do nothing on windows.
  return path;
}

// Returns a string with length or fewer elements, careful to
// not truncate a string mid-surrogate pair.
PathString TruncatePathString(const PathString& original, int length) {
  if (original.size() <= static_cast<size_t>(length))
    return original;
  if (length <= 0)
    return original;
  PathString ret(original.begin(), original.begin() + length);
  COMPILE_ASSERT(sizeof(PathChar) == sizeof(uint16), PathStringNotUTF16);
  PathChar last_char = *ret.rbegin();

  // Values taken from
  // http://en.wikipedia.org/w/index.php?title=UTF-16/UCS-2&oldid=248072840
  if (last_char >= 0xD800 && last_char <= 0xDBFF)
    ret.resize(ret.size() - 1);
  return ret;
}

namespace {
const PathString kWindowsIllegalBaseFilenames[] = {
    L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2",
    L"COM3", L"COM4", L"COM5", L"COM6", L"COM7",
    L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3",
    L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8",
    L"LPT9" };
}

// See: http://msdn.microsoft.com/library/default.asp?url=/library/
// en-us/fileio/fs/naming_a_file.asp
// note that * and ? are not listed on the page as illegal characters,
// but they are.
PathString MakePathComponentOSLegal(const PathString& component) {
  CHECK(!component.empty());
  PathString mutable_component = component;

  // Remove illegal characters.
  for (PathString::iterator i = mutable_component.begin();
       i != mutable_component.end();) {
    if ((PathString::npos != PathString(L"<>:\"/\\|*?").find(*i)) ||
        ((static_cast<unsigned short>(*i) >= 0) &&
         (static_cast<unsigned short>(*i) <= 31))) {
      mutable_component.erase(i);
    } else {
      ++i;
    }
  }

  // Remove trailing spaces or periods.
  while (mutable_component.size() &&
         ((mutable_component.at(mutable_component.size() - 1) == L' ') ||
          (mutable_component.at(mutable_component.size() - 1) == L'.')))
    mutable_component.resize(mutable_component.size() - 1, L' ');

  // Remove a bunch of forbidden names. windows only seems to mind if
  // a forbidden name matches our name exactly (e.g. "prn") or if the
  // name is the forbidden name, followed by a dot, followed by anything
  // (e.g., "prn.anything.foo.bar")

  // From this point out, we break mutable_component into two strings, and
  // use them this way: we save anything after and including the first dot
  // (usually the extension) and only mess with stuff before the first dot.
  PathString::size_type first_dot = mutable_component.find_first_of(L'.');
  if (PathString::npos == first_dot)
    first_dot = mutable_component.size();
  PathString sub = mutable_component.substr(0, first_dot);
  PathString postsub = mutable_component.substr(first_dot);
  CHECK(sub + postsub == mutable_component);
  for (int i = 0; i < ARRAYSIZE(kWindowsIllegalBaseFilenames); i++) {
    // ComparePathNames(a, b) == 0 -> same
    if (syncable::ComparePathNames(kWindowsIllegalBaseFilenames[i], sub) == 0) {
      sub.append(L"~1");
      break;
    }
  }
  if ((L"" == sub) && (L"" == postsub)) {
    sub = L"~1";
  }

  // Return the new name, only if it differs from the original.
  if ((sub + postsub) == component)
    return L"";
  return (sub + postsub);
}
