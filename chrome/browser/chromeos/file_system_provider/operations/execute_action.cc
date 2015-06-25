// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/execute_action.h"

#include <algorithm>
#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

ExecuteAction::ExecuteAction(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& entry_path,
    const std::string& action_id,
    const storage::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      entry_path_(entry_path),
      action_id_(action_id),
      callback_(callback) {
}

ExecuteAction::~ExecuteAction() {
}

bool ExecuteAction::Execute(int request_id) {
  using extensions::api::file_system_provider::ExecuteActionRequestedOptions;

  ExecuteActionRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.entry_path = entry_path_.AsUTF8Unsafe();
  options.action_id = action_id_;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_EXECUTE_ACTION_REQUESTED,
      extensions::api::file_system_provider::OnExecuteActionRequested::
          kEventName,
      extensions::api::file_system_provider::OnExecuteActionRequested::Create(
          options));
}

void ExecuteAction::OnSuccess(int /* request_id */,
                              scoped_ptr<RequestValue> result,
                              bool has_more) {
  callback_.Run(base::File::FILE_OK);
}

void ExecuteAction::OnError(int /* request_id */,
                            scoped_ptr<RequestValue> /* result */,
                            base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
