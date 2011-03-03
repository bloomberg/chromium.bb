// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_file_message_filter.h"

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/pepper_file_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/child_process_security_policy.h"
#include "ipc/ipc_platform_file.h"
#include "webkit/plugins/ppapi/file_path.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

// Used to check if the renderer has permission for the requested operation.
// TODO(viettrungluu): Verify these. They don't necessarily quite make sense,
// but it seems to be approximately what the file system code does.
const int kReadPermissions = base::PLATFORM_FILE_OPEN |
                             base::PLATFORM_FILE_READ |
                             base::PLATFORM_FILE_EXCLUSIVE_READ;
const int kWritePermissions = base::PLATFORM_FILE_OPEN |
                              base::PLATFORM_FILE_CREATE |
                              base::PLATFORM_FILE_CREATE_ALWAYS |
                              base::PLATFORM_FILE_WRITE |
                              base::PLATFORM_FILE_EXCLUSIVE_WRITE |
                              base::PLATFORM_FILE_TRUNCATE |
                              base::PLATFORM_FILE_WRITE_ATTRIBUTES;

PepperFileMessageFilter::PepperFileMessageFilter(int child_id,
                                                 Profile* profile)
    : child_id_(child_id) {
  pepper_path_ = profile->GetPath().Append(FILE_PATH_LITERAL("Pepper Data"));
}

PepperFileMessageFilter::~PepperFileMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void PepperFileMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == PepperFileMsgStart)
    *thread = BrowserThread::FILE;
}

bool PepperFileMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperFileMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(PepperFileMsg_OpenFile, OnOpenFile)
    IPC_MESSAGE_HANDLER(PepperFileMsg_RenameFile, OnRenameFile)
    IPC_MESSAGE_HANDLER(PepperFileMsg_DeleteFileOrDir, OnDeleteFileOrDir)
    IPC_MESSAGE_HANDLER(PepperFileMsg_CreateDir, OnCreateDir)
    IPC_MESSAGE_HANDLER(PepperFileMsg_QueryFile, OnQueryFile)
    IPC_MESSAGE_HANDLER(PepperFileMsg_GetDirContents, OnGetDirContents)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void PepperFileMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

// Called on the FILE thread:
void PepperFileMessageFilter::OnOpenFile(
    const webkit::ppapi::PepperFilePath& path,
    int flags,
    base::PlatformFileError* error,
    IPC::PlatformFileForTransit* file) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path, flags);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    *file = IPC::InvalidPlatformFileForTransit();
    return;
  }

  base::PlatformFile file_handle = base::CreatePlatformFile(
      full_path, flags, NULL, error);

  if (*error != base::PLATFORM_FILE_OK) {
    *file = IPC::InvalidPlatformFileForTransit();
    return;
  }

  // Make sure we didn't try to open a directory: directory fd shouldn't pass
  // to untrusted processes because they open security holes.
  base::PlatformFileInfo info;
  if (!base::GetPlatformFileInfo(file_handle, &info) || info.is_directory) {
    // When in doubt, throw it out.
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    *file = IPC::InvalidPlatformFileForTransit();
    return;
  }

#if defined(OS_WIN)
  // Duplicate the file handle so that the renderer process can access the file.
  if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                       peer_handle(), file, 0, false,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    // file_handle is closed whether or not DuplicateHandle succeeds.
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    *file = INVALID_HANDLE_VALUE;
  }
#else
  *file = base::FileDescriptor(file_handle, true);
#endif
}

void PepperFileMessageFilter::OnRenameFile(
    const webkit::ppapi::PepperFilePath& from_path,
    const webkit::ppapi::PepperFilePath& to_path,
    base::PlatformFileError* error) {
  FilePath from_full_path = ValidateAndConvertPepperFilePath(from_path,
                                                             kWritePermissions);
  FilePath to_full_path = ValidateAndConvertPepperFilePath(to_path,
                                                           kWritePermissions);
  if (from_full_path.empty() || to_full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::Move(from_full_path, to_full_path);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnDeleteFileOrDir(
    const webkit::ppapi::PepperFilePath& path,
    bool recursive,
    base::PlatformFileError* error) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path,
                                                        kWritePermissions);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::Delete(full_path, recursive);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnCreateDir(
    const webkit::ppapi::PepperFilePath& path,
    base::PlatformFileError* error) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path,
                                                        kWritePermissions);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::CreateDirectory(full_path);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnQueryFile(
    const webkit::ppapi::PepperFilePath& path,
    base::PlatformFileInfo* info,
    base::PlatformFileError* error) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path, kReadPermissions);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::GetFileInfo(full_path, info);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnGetDirContents(
    const webkit::ppapi::PepperFilePath& path,
    webkit::ppapi::DirContents* contents,
    base::PlatformFileError* error) {
  FilePath full_path = ValidateAndConvertPepperFilePath(path, kReadPermissions);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  contents->clear();

  file_util::FileEnumerator enumerator(
      full_path, false,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::INCLUDE_DOT_DOT));

  while (!enumerator.Next().empty()) {
    file_util::FileEnumerator::FindInfo info;
    enumerator.GetFindInfo(&info);
    webkit::ppapi::DirEntry entry = {
      file_util::FileEnumerator::GetFilename(info),
      file_util::FileEnumerator::IsDirectory(info)
    };
    contents->push_back(entry);
  }

  *error = base::PLATFORM_FILE_OK;
}

FilePath PepperFileMessageFilter::ValidateAndConvertPepperFilePath(
    const webkit::ppapi::PepperFilePath& pepper_path, int flags) {
  FilePath file_path;  // Empty path returned on error.
  switch(pepper_path.domain()) {
    case webkit::ppapi::PepperFilePath::DOMAIN_ABSOLUTE:
// TODO(viettrungluu): This could be dangerous if not 100% right, so let's be
// conservative and only enable it when requested.
#if defined(ENABLE_FLAPPER_HACKS)
      if (pepper_path.path().IsAbsolute() &&
          ChildProcessSecurityPolicy::GetInstance()->HasPermissionsForFile(
              child_id(), pepper_path.path(), flags))
        file_path = pepper_path.path();
#else
      NOTIMPLEMENTED();
#endif  // ENABLE_FLAPPER_HACKS
      break;
    case webkit::ppapi::PepperFilePath::DOMAIN_MODULE_LOCAL:
      if (!pepper_path.path().IsAbsolute() &&
          !pepper_path.path().ReferencesParent())
        file_path = pepper_path_.Append(pepper_path.path());
      break;
    default:
      NOTREACHED();
      break;
  }
  return file_path;
}
