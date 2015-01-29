// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_HISTORY_DATABASE_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CORE_HISTORY_DATABASE_TEST_UTILS_H_

#include "base/compiler_specific.h"

namespace base {
class FilePath;
}

namespace history {

// Create the test database at |db_path| from the golden file at |ascii_path| in
// the "history" subdir of the components test data dir.
WARN_UNUSED_RESULT bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                                              const char* ascii_path);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_HISTORY_DATABASE_TEST_UTILS_H_
