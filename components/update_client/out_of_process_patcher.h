// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OUT_OF_PROCESS_PATCHER_H_
#define COMPONENTS_UPDATE_CLIENT_OUT_OF_PROCESS_PATCHER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace update_client {

// An interface an embedder can implement to enable out-of-process patching.
class OutOfProcessPatcher
    : public base::RefCountedThreadSafe<OutOfProcessPatcher> {
 public:
  virtual void Patch(
      const std::string& operation,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& input_abs_path,
      const base::FilePath& patch_abs_path,
      const base::FilePath& output_abs_path,
      const base::Callback<void(int result)>& callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<OutOfProcessPatcher>;

  virtual ~OutOfProcessPatcher() {}
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OUT_OF_PROCESS_PATCHER_H_
