// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <iterator>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

#include "components/update_client/component.h"
#include "components/update_client/update_client.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the recovery payload.
// TODO(sorin): inject this value using the Configurator.
constexpr uint8_t kPublicKeySHA256[] = {
    0xd8, 0x40, 0x46, 0x12, 0xd2, 0x66, 0x9a, 0xf1, 0x0e, 0x64, 0x98,
    0x36, 0x9d, 0xd5, 0x46, 0xe4, 0x52, 0xe8, 0x9b, 0x2d, 0x9b, 0x76,
    0x84, 0x06, 0xc5, 0x5c, 0xb3, 0xb8, 0xf4, 0xc5, 0x80, 0x40};

std::vector<uint8_t> GetHash() {
  return std::vector<uint8_t>{std::begin(kPublicKeySHA256),
                              std::end(kPublicKeySHA256)};
}

}  // namespace

namespace update_client {

ActionRunner::ActionRunner(
    const Component& component,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : component_(component),
      task_runner_(task_runner),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

ActionRunner::~ActionRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ActionRunner::Run(const Callback& run_complete) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  run_complete_ = run_complete;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&ActionRunner::Unpack, base::Unretained(this)));
}

void ActionRunner::Unpack() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const auto& installer = component_.crx_component().installer;

  base::FilePath file_path;
  installer->GetInstalledFile(component_.action_run(), &file_path);

  auto unpacker = base::MakeRefCounted<ComponentUnpacker>(
      GetHash(), file_path, installer, nullptr, task_runner_);

  unpacker->Unpack(
      base::Bind(&ActionRunner::UnpackComplete, base::Unretained(this)));
}

void ActionRunner::UnpackComplete(const ComponentUnpacker::Result& result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(sorin): invoke the command runner here. For now, just return
  // canned values for the unit test.
  base::DeleteFile(result.unpack_path, true);
  main_task_runner_->PostTask(FROM_HERE, base::Bind(run_complete_, true, 1, 2));
}

}  // namespace update_client
