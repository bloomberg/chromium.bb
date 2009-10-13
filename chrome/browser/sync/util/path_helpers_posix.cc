// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>

#include <algorithm>
#include <pwd.h>
#include <string.h>

#include "base/logging.h"
#include "base/port.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/path_helpers.h"

#if ((!defined(OS_LINUX)) && (!defined(OS_MACOSX)))
#error Compile this file on Mac OS X or Linux only.
#endif

PathString ExpandTilde(const PathString& path) {
  if (path.empty())
    return path;
  if ('~' != path[0])
    return path;
  PathString ret;
  // TODO(sync): Consider using getpwuid_r.
  ret.insert(0, getpwuid(getuid())->pw_dir);
  ret.append(++path.begin(), path.end());
  return ret;
}

namespace {
// TODO(sync): We really should use char[].
  std::string cache_dir_;
}

void set_cache_dir(std::string cache_dir) {
  CHECK(cache_dir_.empty());
  cache_dir_ = cache_dir;
}

std::string get_cache_dir() {
  CHECK(!cache_dir_.empty());
  return cache_dir_;
}

// On Posix, PathStrings are UTF-8, not UTF-16 as they are on Windows. Thus,
// this function is different from the Windows version.
PathString TruncatePathString(const PathString& original, int length) {
  if (original.size() <= static_cast<size_t>(length))
    return original;
  if (length <= 0)
    return original;
  PathString ret(original.begin(), original.begin() + length);
  COMPILE_ASSERT(sizeof(PathChar) == sizeof(uint8), PathStringNotUTF8);
  PathString::reverse_iterator last_char = ret.rbegin();

  // Values taken from
  // http://en.wikipedia.org/w/index.php?title=UTF-8&oldid=252875566
  if (0 == (*last_char & 0x80))
    return ret;

  for (; last_char != ret.rend(); ++last_char) {
    if (0 == (*last_char & 0x80)) {
      // Got malformed UTF-8; bail.
      return ret;
    }
    if (0 == (*last_char & 0x40)) {
      // Got another trailing byte.
      continue;
    }
    break;
  }

  if (ret.rend() == last_char) {
    // We hit the beginning of the string. bail.
    return ret;
  }

  int last_codepoint_len = last_char - ret.rbegin() + 1;

  if (((0xC0 == (*last_char & 0xE0)) && (2 == last_codepoint_len)) ||
      ((0xE0 == (*last_char & 0xF0)) && (3 == last_codepoint_len)) ||
      ((0xF0 == (*last_char & 0xF8)) && (4 == last_codepoint_len))) {
    // Valid utf-8.
    return ret;
  }

  // Invalid utf-8. chop off last "codepoint" and return.
  ret.resize(ret.size() - last_codepoint_len);
  return ret;
}

// Convert /s to :s.
PathString MakePathComponentOSLegal(const PathString& component) {
  if (PathString::npos == component.find("/"))
    return PSTR("");
  PathString new_name(component);
  std::replace(new_name.begin(), new_name.end(), '/', ':');
  return new_name;
}

string LastPathSegment(const string& path) {
  string str(path);
  string::size_type final_slash = str.find_last_of('/');
  if (string::npos != final_slash && final_slash == str.length() - 1
      && str.length() > 1) {
    str.erase(final_slash);
    final_slash = str.find_last_of('/');
  }
  if (string::npos == final_slash)
    return str;
  str.erase(0, final_slash + 1);
  return str;
}

PathString AppendSlash(const PathString& path) {
  if ((!path.empty()) && (*path.rbegin() != '/')) {
    return path + '/';
  }
  return path;
}

