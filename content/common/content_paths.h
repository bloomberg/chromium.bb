// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_PATHS_H_
#define CONTENT_COMMON_CONTENT_PATHS_H_
#pragma once

// This file declares path keys for the content module.  These can be used with
// the PathService to access various special directories and files.

namespace content {

enum {
  PATH_START = 4000,

  // Path and filename to the executable to use for child processes.
  CHILD_PROCESS_EXE = PATH_START,

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_PATHS_H_
