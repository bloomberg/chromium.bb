// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/path_helpers.h"

#include <Shlwapi.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/port.h"
#include "build/build_config.h"
#include "chrome/browser/sync/syncable/syncable.h"

#ifndef OS_WIN
#error Compile this file on Windows only.
#endif

using std::string;

namespace {
const PathString kWindowsIllegalBaseFilenames[] = {
    "CON", "PRN", "AUX", "NUL", "COM1", "COM2",
    "COM3", "COM4", "COM5", "COM6", "COM7",
    "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8",
    "LPT9" };
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
    if ((PathString::npos != PathString("<>:\"/\\|*?").find(*i)) ||
        ((static_cast<unsigned short>(*i) >= 0) &&
         (static_cast<unsigned short>(*i) <= 31))) {
      mutable_component.erase(i);
    } else {
      ++i;
    }
  }

  // Remove trailing spaces or periods.
  while (mutable_component.size() &&
         ((mutable_component.at(mutable_component.size() - 1) == ' ') ||
          (mutable_component.at(mutable_component.size() - 1) == '.')))
    mutable_component.resize(mutable_component.size() - 1, ' ');

  // Remove a bunch of forbidden names. windows only seems to mind if
  // a forbidden name matches our name exactly (e.g. "prn") or if the name is
  // the forbidden name, followed by a dot, followed by anything
  // (e.g., "prn.anything.foo.bar")

  // From this point out, we break mutable_component into two strings, and use
  // them this way: we save anything after and including the first dot (usually
  // the extension) and only mess with stuff before the first dot.
  PathString::size_type first_dot = mutable_component.find_first_of('.');
  if (PathString::npos == first_dot)
    first_dot = mutable_component.size();
  PathString sub = mutable_component.substr(0, first_dot);
  PathString postsub = mutable_component.substr(first_dot);
  CHECK(sub + postsub == mutable_component);
  for (int i = 0; i < ARRAYSIZE(kWindowsIllegalBaseFilenames); i++) {
    // ComparePathNames(a, b) == 0 -> same
    if (syncable::ComparePathNames(kWindowsIllegalBaseFilenames[i], sub) == 0) {
      sub.append("~1");
      break;
    }
  }
  if (("" == sub) && ("" == postsub)) {
    sub = "~1";
  }

  // Return the new name, only if it differs from the original.
  if ((sub + postsub) == component)
    return "";
  return (sub + postsub);
}
