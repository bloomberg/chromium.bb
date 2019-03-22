// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_requests_proxy.h"

#include <sddl.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

namespace {

template <typename ResultType>
void SaveBoolAndCopyableDataCallback(bool* out_success,
                                     ResultType* out_result,
                                     base::OnceClosure quit_closure,
                                     bool success,
                                     const ResultType& result) {
  *out_success = success;
  *out_result = result;
  std::move(quit_closure).Run();
}

void SaveOpenReadOnlyRegistryCallback(uint32_t* out_return_code,
                                      HANDLE* result_holder,
                                      base::OnceClosure quit_closure,
                                      uint32_t return_code,
                                      HANDLE handle) {
  *out_return_code = return_code;
  *result_holder = std::move(handle);
  std::move(quit_closure).Run();
}

void SaveBoolAndScheduledTasksCallback(
    bool* out_success,
    std::vector<TaskScheduler::TaskInfo>* out_tasks,
    base::OnceClosure quit_closure,
    bool in_success,
    std::vector<mojom::ScheduledTaskPtr> in_tasks) {
  out_tasks->clear();
  out_tasks->reserve(in_tasks.size());
  for (mojom::ScheduledTaskPtr& in_task : in_tasks) {
    TaskScheduler::TaskInfo out_task;
    out_task.name = std::move(in_task->name);
    out_task.description = std::move(in_task->description);
    out_task.exec_actions.reserve(in_task->actions.size());

    for (mojom::ScheduledTaskActionPtr& in_task_action : in_task->actions) {
      TaskScheduler::TaskExecAction out_action;
      out_action.application_path = std::move(in_task_action->path);
      out_action.working_dir = std::move(in_task_action->working_dir);
      out_action.arguments = std::move(in_task_action->arguments);
      out_task.exec_actions.push_back(std::move(out_action));
    }
    out_tasks->push_back(std::move(out_task));
  }

  *out_success = in_success;
  std::move(quit_closure).Run();
}

void SaveUserInformationCallback(bool* out_success,
                                 mojom::UserInformation* out_result,
                                 base::OnceClosure quit_closure,
                                 bool in_success,
                                 mojom::UserInformationPtr in_result) {
  *out_success = in_success;
  out_result->name = std::move(in_result->name);
  out_result->domain = std::move(in_result->domain);
  out_result->account_type = in_result->account_type;
  std::move(quit_closure).Run();
}

}  // namespace

EngineRequestsProxy::EngineRequestsProxy(
    mojom::EngineRequestsAssociatedPtr engine_requests_ptr,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : engine_requests_ptr_(std::move(engine_requests_ptr)),
      task_runner_(task_runner) {}

void EngineRequestsProxy::UnbindRequestsPtr() {
  engine_requests_ptr_.reset();
}

bool EngineRequestsProxy::GetKnownFolderPath(mojom::KnownFolder folder_id,
                                             base::FilePath* folder_path) {
  if (folder_path == nullptr) {
    LOG(ERROR) << "GetKnownFolderPathCallback given a null folder_path";
    return false;
  }
  bool result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetKnownFolderPath,
                     base::Unretained(this), folder_id),
      base::BindOnce(&SaveBoolAndCopyableDataCallback<base::FilePath>, &result,
                     folder_path));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetProcesses(
    std::vector<base::ProcessId>* processes) {
  if (processes == nullptr) {
    LOG(ERROR) << "GetProcessesCallback received a null processes";
    return false;
  }
  bool success = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetProcesses,
                     base::Unretained(this)),
      base::BindOnce(
          &SaveBoolAndCopyableDataCallback<std::vector<base::ProcessId>>,
          &success, processes));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return success;
}

bool EngineRequestsProxy::GetTasks(
    std::vector<TaskScheduler::TaskInfo>* task_list) {
  if (task_list == nullptr) {
    LOG(ERROR) << "GetTasksCallback received a null task_list";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetTasks,
                     base::Unretained(this)),
      base::BindOnce(&SaveBoolAndScheduledTasksCallback, &result, task_list));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetProcessImagePath(base::ProcessId pid,
                                              base::FilePath* image_path) {
  if (image_path == nullptr) {
    LOG(ERROR) << "GetProcessImagePathCallback received a null file_info";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetProcessImagePath,
                     base::Unretained(this), pid),
      base::BindOnce(&SaveBoolAndCopyableDataCallback<base::FilePath>, &result,
                     image_path));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetLoadedModules(
    base::ProcessId pid,
    std::vector<base::string16>* modules) {
  if (modules == nullptr) {
    LOG(ERROR) << "GetLoadedModulesCallback received a null modules";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetLoadedModules,
                     base::Unretained(this), pid),
      base::BindOnce(
          &SaveBoolAndCopyableDataCallback<std::vector<base::string16>>,
          &result, modules));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetProcessCommandLine(base::ProcessId pid,
                                                base::string16* command_line) {
  if (command_line == nullptr) {
    LOG(ERROR) << "GetProcessCommandLineCallback received a null command_line";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetProcessCommandLine,
                     base::Unretained(this), pid),
      base::BindOnce(&SaveBoolAndCopyableDataCallback<base::string16>, &result,
                     command_line));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetUserInfoFromSID(
    const SID* const sid,
    mojom::UserInformation* user_info) {
  if (user_info == nullptr) {
    LOG(ERROR) << "GetUserInfoFromSIDCallback given a null buffer";
    return false;
  }

  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetUserInfoFromSID,
                     base::Unretained(this), sid),
      base::BindOnce(&SaveUserInformationCallback, &result, user_info));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

uint32_t EngineRequestsProxy::OpenReadOnlyRegistry(
    HANDLE root_key,
    const base::string16& sub_key,
    uint32_t dw_access,
    HANDLE* registry_handle) {
  uint32_t return_code;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxOpenReadOnlyRegistry,
                     base::Unretained(this), root_key, sub_key, dw_access),
      base::BindOnce(&SaveOpenReadOnlyRegistryCallback, &return_code,
                     registry_handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return return_code;
}

uint32_t EngineRequestsProxy::NtOpenReadOnlyRegistry(
    HANDLE root_key,
    const String16EmbeddedNulls& sub_key,
    uint32_t dw_access,
    HANDLE* registry_handle) {
  uint32_t return_code;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxNtOpenReadOnlyRegistry,
                     base::Unretained(this), root_key, sub_key, dw_access),
      base::BindOnce(&SaveOpenReadOnlyRegistryCallback, &return_code,
                     registry_handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    if (registry_handle)
      *registry_handle = INVALID_HANDLE_VALUE;
    return call_status.error_code;
  }
  return return_code;
}

EngineRequestsProxy::~EngineRequestsProxy() = default;

MojoCallStatus EngineRequestsProxy::SandboxGetKnownFolderPath(
    mojom::KnownFolder folder_id,
    mojom::EngineRequests::SandboxGetKnownFolderPathCallback result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxGetKnownFolderPath called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxGetKnownFolderPath(folder_id,
                                                  std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetProcesses(
    mojom::EngineRequests::SandboxGetProcessesCallback result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxGetProcesses called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxGetProcesses(std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetTasks(
    mojom::EngineRequests::SandboxGetTasksCallback result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxGetTasks called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxGetTasks(std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetProcessImagePath(
    base::ProcessId pid,
    mojom::EngineRequests::SandboxGetProcessImagePathCallback result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxGetProcessImagePath called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxGetProcessImagePath(pid,
                                                   std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetLoadedModules(
    base::ProcessId pid,
    mojom::EngineRequests::SandboxGetLoadedModulesCallback result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxGetLoadedModules called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxGetLoadedModules(pid,
                                                std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetProcessCommandLine(
    base::ProcessId pid,
    mojom::EngineRequestsProxy::SandboxGetProcessCommandLineCallback
        result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxGetProcessCommandLine called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxGetProcessCommandLine(
      pid, std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetUserInfoFromSID(
    const SID* const sid,
    mojom::EngineRequests::SandboxGetUserInfoFromSIDCallback result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxGetUserInfoFromSID called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  wchar_t* sid_buffer = nullptr;
  if (!::ConvertSidToStringSidW(const_cast<SID*>(sid), &sid_buffer)) {
    PLOG(ERROR) << "Unable to convert |sid| to a string.";
    return MojoCallStatus::Failure(SandboxErrorCode::BAD_SID);
  }

  auto mojom_string_sid = mojom::StringSid::New(sid_buffer);
  LocalFree(sid_buffer);

  engine_requests_ptr_->SandboxGetUserInfoFromSID(std::move(mojom_string_sid),
                                                  std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxOpenReadOnlyRegistry(
    HANDLE root_key,
    const base::string16& sub_key,
    uint32_t dw_access,
    mojom::EngineRequests::SandboxOpenReadOnlyRegistryCallback
        result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxOpenReadOnlyRegistry called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxOpenReadOnlyRegistry(
      root_key, sub_key, dw_access, std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxNtOpenReadOnlyRegistry(
    HANDLE root_key,
    const String16EmbeddedNulls& sub_key,
    uint32_t dw_access,
    mojom::EngineRequests::SandboxNtOpenReadOnlyRegistryCallback
        result_callback) {
  if (!engine_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxNtOpenReadOnlyRegistry called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_ptr_->SandboxNtOpenReadOnlyRegistry(
      root_key, sub_key, dw_access, std::move(result_callback));

  return MojoCallStatus::Success();
}

}  // namespace chrome_cleaner
