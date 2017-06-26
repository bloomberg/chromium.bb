// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/get_operation_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"

namespace offline_pages {

namespace {

// Will hold the actual SQL implementation for marking a GetOperation attempt in
// the database.
static int MarkOperationAttemptStarted(const std::string& operation_name) {
  NOTIMPLEMENTED();
  return 1;
}

// TODO(fgorski): replace this with the SQL executor.
static void Execute(base::RepeatingCallback<int()> command_callback,
                    base::OnceCallback<void(int)> result_callback) {
  std::move(result_callback).Run(command_callback.Run());
}
}  // namespace

GetOperationTask::GetOperationTask(
    const std::string& operation_name,
    PrefetchNetworkRequestFactory* request_factory,
    const PrefetchRequestFinishedCallback& callback)
    : operation_name_(operation_name),
      request_factory_(request_factory),
      callback_(callback),
      weak_factory_(this) {}

GetOperationTask::~GetOperationTask() {}

void GetOperationTask::Run() {
  Execute(base::BindRepeating(&MarkOperationAttemptStarted, operation_name_),
          base::BindOnce(&GetOperationTask::StartGetOperation,
                         weak_factory_.GetWeakPtr()));
}

void GetOperationTask::StartGetOperation(int updated_entry_count) {
  request_factory_->MakeGetOperationRequest(operation_name_, callback_);
  TaskComplete();
}

}  // namespace offline_pages
