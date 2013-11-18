// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_TEST_FILE_RUNNER_H_
#define GIN_TEST_FILE_RUNNER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "gin/modules/module_runner_delegate.h"
#include "gin/runner.h"

namespace gin {

class FileRunnerDelegate : public ModuleRunnerDelegate {
 public:
  FileRunnerDelegate();
  virtual ~FileRunnerDelegate();

 private:
  // From ModuleRunnerDelegate:
  virtual void UnhandledException(Runner* runner, TryCatch& try_catch) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FileRunnerDelegate);
};

void RunTestFromFile(const base::FilePath& path, FileRunnerDelegate* delegate);

}  // namespace gin

#endif  // GIN_TEST_FILE_RUNNER_H_
