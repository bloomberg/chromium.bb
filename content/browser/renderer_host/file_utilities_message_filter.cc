// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/file_utilities_message_filter.h"

#include "base/file_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/common/file_utilities_messages.h"
#include "content/public/browser/content_browser_client.h"

using content::BrowserThread;

FileUtilitiesMessageFilter::FileUtilitiesMessageFilter(int process_id)
    : process_id_(process_id) {
}

FileUtilitiesMessageFilter::~FileUtilitiesMessageFilter() {
}

void FileUtilitiesMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == FileUtilitiesMsg_RevealFolderInOS::ID)
    *thread = BrowserThread::UI;
  else if (IPC_MESSAGE_CLASS(message) == FileUtilitiesMsgStart)
    *thread = BrowserThread::FILE;
}

bool FileUtilitiesMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                   bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(FileUtilitiesMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(FileUtilitiesMsg_GetFileSize, OnGetFileSize)
    IPC_MESSAGE_HANDLER(FileUtilitiesMsg_GetFileModificationTime,
                        OnGetFileModificationTime)
    IPC_MESSAGE_HANDLER(FileUtilitiesMsg_OpenFile, OnOpenFile)
    IPC_MESSAGE_HANDLER(FileUtilitiesMsg_RevealFolderInOS, OnRevealFolderInOS)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FileUtilitiesMessageFilter::OnGetFileSize(const FilePath& path,
                                               int64* result) {
  *result = -1;

  // Get file size only when the child process has been granted permission to
  // upload the file.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path)) {
    return;
  }

  base::PlatformFileInfo file_info;
  file_info.size = 0;
  if (file_util::GetFileInfo(path, &file_info))
    *result = file_info.size;
}

void FileUtilitiesMessageFilter::OnGetFileModificationTime(
    const FilePath& path, base::Time* result) {
  *result = base::Time();

  // Get file modification time only when the child process has been granted
  // permission to upload the file.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path)) {
    return;
  }

  base::PlatformFileInfo file_info;
  file_info.size = 0;
  if (file_util::GetFileInfo(path, &file_info))
    *result = file_info.last_modified;
}

void FileUtilitiesMessageFilter::OnOpenFile(
    const FilePath& path,
    int mode,
    IPC::PlatformFileForTransit* result) {
  // Open the file only when the child process has been granted permission to
  // upload the file.
  // TODO(jianli): Do we need separate permission to control opening the file?
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
          process_id_, path)) {
#if defined(OS_WIN)
    *result = base::kInvalidPlatformFileValue;
#elif defined(OS_POSIX)
    *result = base::FileDescriptor(base::kInvalidPlatformFileValue, true);
#endif
    return;
  }

  base::PlatformFile file_handle = base::CreatePlatformFile(
      path,
      (mode == 0) ? (base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ)
                  : (base::PLATFORM_FILE_CREATE_ALWAYS |
                        base::PLATFORM_FILE_WRITE),
      NULL, NULL);

#if defined(OS_WIN)
  // Duplicate the file handle so that the renderer process can access the file.
  if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                       peer_handle(), result, 0, false,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    // file_handle is closed whether or not DuplicateHandle succeeds.
    *result = INVALID_HANDLE_VALUE;
  }
#else
  *result = base::FileDescriptor(file_handle, true);
#endif
}

void FileUtilitiesMessageFilter::OnRevealFolderInOS(const FilePath& path) {
  // Only honor the request if appropriate permissions are granted.
  if (ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(process_id_,
                                                                 path))
    content::GetContentClient()->browser()->OpenItem(path);
}
