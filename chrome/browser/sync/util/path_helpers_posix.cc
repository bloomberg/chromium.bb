// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/path_helpers.h"

#include "build/build_config.h"

#if ((!defined(OS_LINUX)) && (!defined(OS_MACOSX)))
#error Compile this file on Mac OS X or Linux only.
#endif

// Convert /s to :s.
PathString MakePathComponentOSLegal(const PathString& component) {
  if (PathString::npos == component.find("/"))
    return PSTR("");
  PathString new_name(component);
  std::replace(new_name.begin(), new_name.end(), '/', ':');
  return new_name;
}
