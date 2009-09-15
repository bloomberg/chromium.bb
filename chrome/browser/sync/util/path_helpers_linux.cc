// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>

#include <glib.h>
#include <string.h>

#include "base/logging.h"
#include "base/port.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/path_helpers.h"

#ifndef OS_LINUX
#error Compile this file on Linux only.
#endif

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

PathString GetFullPath(const PathString& path) {
  // TODO(sync): Not sure what the base of the relative path should be on
  // linux.
  return path;
}

PathString AppendSlash(const PathString& path) {
  if ((!path.empty()) && (*path.rbegin() != '/')) {
    return path + '/';
  }
  return path;
}

PathString LowercasePath(const PathString& path) {
  gchar* ret = g_utf8_strdown(path.c_str(), -1);
  PathString retstr(ret);
  g_free(ret);
  return retstr;
}
