// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_TEST_FILE_RUNNER_H_
#define GIN_TEST_FILE_RUNNER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "gin/runner.h"

namespace gin {

class FileRunnerDelegate : public RunnerDelegate {
 public:
  FileRunnerDelegate();
  virtual ~FileRunnerDelegate();
  virtual v8::Handle<v8::ObjectTemplate> GetGlobalTemplate(
      Runner* runner) OVERRIDE;
  virtual void DidCreateContext(Runner* runner) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileRunnerDelegate);
};

void RunTestFromFile(const base::FilePath& path, RunnerDelegate* delegate);

}  // namespace gin

#endif  // GIN_TEST_FILE_RUNNER_H_
