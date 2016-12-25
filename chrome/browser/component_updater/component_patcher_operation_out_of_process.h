// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_OUT_OF_PROCESS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_OUT_OF_PROCESS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/component_patcher_operation.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace component_updater {

class PatchHost;

// Implements the DeltaUpdateOpPatch out-of-process patching.
class ChromeOutOfProcessPatcher : public update_client::OutOfProcessPatcher {
 public:
  ChromeOutOfProcessPatcher();

  // DeltaUpdateOpPatch::OutOfProcessPatcher implementation.
  void Patch(const std::string& operation,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const base::FilePath& input_abs_path,
             const base::FilePath& patch_abs_path,
             const base::FilePath& output_abs_path,
             base::Callback<void(int result)> callback) override;

 private:
  ~ChromeOutOfProcessPatcher() override;

  scoped_refptr<PatchHost> host_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOutOfProcessPatcher);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_OUT_OF_PROCESS_H_
