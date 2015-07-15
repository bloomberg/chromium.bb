// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_SCOPED_USER_DATA_DIR_H_
#define MOJO_RUNNER_SCOPED_USER_DATA_DIR_H_

#include "base/files/scoped_temp_dir.h"

namespace mojo {
namespace runner {

// A scoped class which owns a ScopedTempDir if --use-temporary-user-data-dir
// is set. If it is, also modifies the command line so that --user-data-dir
// points to the temporary dir.
class ScopedUserDataDir {
 public:
  ScopedUserDataDir();
  ~ScopedUserDataDir();

 private:
  base::ScopedTempDir temp_dir_;
};

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_SCOPED_USER_DATA_DIR_H_
