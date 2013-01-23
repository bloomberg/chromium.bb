// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_message_filter.h"

#include "base/file_util.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/shell/shell_messages.h"
#include "webkit/fileapi/isolated_context.h"

namespace content {

ShellMessageFilter::ShellMessageFilter(int render_process_id)
    : render_process_id_(render_process_id) {
}

ShellMessageFilter::~ShellMessageFilter() {
}

bool ShellMessageFilter::OnMessageReceived(const IPC::Message& message,
                                           bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ShellMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ReadFileToString, OnReadFileToString)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_RegisterIsolatedFileSystem,
                        OnRegisterIsolatedFileSystem)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ShellMessageFilter::OnReadFileToString(const FilePath& local_file,
                                            std::string* contents) {
  file_util::ReadFileToString(local_file, contents);
}

void ShellMessageFilter::OnRegisterIsolatedFileSystem(
    const std::vector<FilePath>& absolute_filenames,
    std::string* filesystem_id) {
  fileapi::IsolatedContext::FileInfoSet files;
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  for (size_t i = 0; i < absolute_filenames.size(); ++i) {
    files.AddPath(absolute_filenames[i], NULL);
    if (!policy->CanReadFile(render_process_id_, absolute_filenames[i]))
      policy->GrantReadFile(render_process_id_, absolute_filenames[i]);
  }
  *filesystem_id =
      fileapi::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  policy->GrantReadFileSystem(render_process_id_, *filesystem_id);
}

}  // namespace content
