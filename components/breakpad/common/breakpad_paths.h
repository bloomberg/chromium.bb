// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREAKPAD_COMMON_BREAKPAD_PATHS_H_
#define COMPONENTS_BREAKPAD_COMMON_BREAKPAD_PATHS_H_

namespace breakpad {

enum {
  PATH_START = 8000,

  // Directory where crash dumps are written. The embedder of the breakpad
  // component has to define DIR_CRASH_DUMPS using PathService::Override.
  DIR_CRASH_DUMPS = PATH_START,

  PATH_END
};

}  // namespace breakpad

#endif  // COMPONENTS_BREAKPAD_COMMON_BREAKPAD_PATHS_H_
