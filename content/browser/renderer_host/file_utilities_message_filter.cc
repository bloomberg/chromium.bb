// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/file_utilities_message_filter.h"

#include "base/files/file_util.h"
#include "base/task_scheduler/lazy_task_runner.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/common/file_utilities_messages.h"

namespace {

base::LazySequencedTaskRunner g_file_utility_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_VISIBLE));

}  // namespace

namespace content {

FileUtilitiesMessageFilter::FileUtilitiesMessageFilter(int process_id)
    : BrowserMessageFilter(FileUtilitiesMsgStart),
      process_id_(process_id) {
}

FileUtilitiesMessageFilter::~FileUtilitiesMessageFilter() {
}

base::TaskRunner* FileUtilitiesMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  if (IPC_MESSAGE_CLASS(message) == FileUtilitiesMsgStart)
    return g_file_utility_task_runner.Get().get();
  return nullptr;
}

bool FileUtilitiesMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FileUtilitiesMessageFilter, message)
    IPC_MESSAGE_HANDLER(FileUtilitiesMsg_GetFileInfo, OnGetFileInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FileUtilitiesMessageFilter::OnGetFileInfo(
    const base::FilePath& path,
    base::File::Info* result,
    base::File::Error* status) {
  *result = base::File::Info();
  *status = base::File::FILE_OK;

  // Get file metadata only when the child process has been granted
  // permission to read the file.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path)) {
    return;
  }

  if (!base::GetFileInfo(path, result))
    *status = base::File::FILE_ERROR_FAILED;
}

}  // namespace content
