// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_file_message_filter.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/file_path.h"
#include "base/process_util.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/pepper_file_messages.h"
#include "ipc/ipc_platform_file.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

PepperFileMessageFilter::PepperFileMessageFilter(
    int child_id, Profile* profile) {
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

bool PepperFileMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperFileMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(PepperFileMsg_OpenFile, OnPepperOpenFile)
    IPC_MESSAGE_HANDLER(PepperFileMsg_RenameFile, OnPepperRenameFile)
    IPC_MESSAGE_HANDLER(PepperFileMsg_DeleteFileOrDir, OnPepperDeleteFileOrDir)
    IPC_MESSAGE_HANDLER(PepperFileMsg_CreateDir, OnPepperCreateDir)
    IPC_MESSAGE_HANDLER(PepperFileMsg_QueryFile, OnPepperQueryFile)
    IPC_MESSAGE_HANDLER(PepperFileMsg_GetDirContents, OnPepperGetDirContents)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void PepperFileMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

// Called on the FILE thread:
void PepperFileMessageFilter::OnPepperOpenFile(
    const FilePath& path,
    int flags,
    base::PlatformFileError* error,
    IPC::PlatformFileForTransit* file) {
  FilePath full_path = MakePepperPath(path);
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

void PepperFileMessageFilter::OnPepperRenameFile(
    const FilePath& path_from,
    const FilePath& path_to,
    base::PlatformFileError* error) {
  FilePath full_path_from = MakePepperPath(path_from);
  FilePath full_path_to = MakePepperPath(path_to);
  if (full_path_from.empty() || full_path_to.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::Move(full_path_from, full_path_to);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnPepperDeleteFileOrDir(
    const FilePath& path,
    bool recursive,
    base::PlatformFileError* error) {
  FilePath full_path = MakePepperPath(path);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::Delete(full_path, recursive);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnPepperCreateDir(
    const FilePath& path,
    base::PlatformFileError* error) {
  FilePath full_path = MakePepperPath(path);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::CreateDirectory(full_path);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnPepperQueryFile(
    const FilePath& path,
    base::PlatformFileInfo* info,
    base::PlatformFileError* error) {
  FilePath full_path = MakePepperPath(path);
  if (full_path.empty()) {
    *error = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    return;
  }

  bool result = file_util::GetFileInfo(full_path, info);
  *error = result ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
}

void PepperFileMessageFilter::OnPepperGetDirContents(
    const FilePath& path,
    webkit::ppapi::DirContents* contents,
    base::PlatformFileError* error) {
  FilePath full_path = MakePepperPath(path);
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

FilePath PepperFileMessageFilter::MakePepperPath(const FilePath& base_path) {
  if (base_path.IsAbsolute() || base_path.ReferencesParent()) {
    return FilePath();
  }
  return pepper_path_.Append(base_path);
}
