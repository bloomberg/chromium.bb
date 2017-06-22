// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/component_unpacker.h"

namespace base {
class CommandLine;
class Process;
class SequencedTaskRunner;
}

namespace update_client {

class Component;

class ActionRunner {
 public:
  using Callback =
      base::Callback<void(bool succeeded, int error_code, int extra_code1)>;

  ActionRunner(const Component& component,
               const scoped_refptr<base::SequencedTaskRunner>& task_runner,
               const std::vector<uint8_t>& key_hash);
  ~ActionRunner();

  void Run(const Callback& run_complete);

 private:
  void Unpack();
  void UnpackComplete(const ComponentUnpacker::Result& result);

  void RunCommand(const base::CommandLine& cmdline);

  base::CommandLine MakeCommandLine(const base::FilePath& unpack_path) const;

  void WaitForCommand(base::Process process);

  const Component& component_;
  const scoped_refptr<base::SequencedTaskRunner>& task_runner_;

  // Contains the key hash of the CRX this object is allowed to run. This value
  // is using during the unpacking of the CRX to verify its integrity.
  const std::vector<uint8_t> key_hash_;

  // Used to post callbacks to the main thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  Callback run_complete_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(ActionRunner);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
