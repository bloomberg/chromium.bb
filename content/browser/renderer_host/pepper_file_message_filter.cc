// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_file_message_filter.h"

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/pepper_file_messages.h"
#include "content/browser/browser_thread.h"
#include "ipc/ipc_platform_file.h"
#include "webkit/plugins/ppapi/file_path.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {

FilePath ConvertPepperFilePath(
    const webkit::ppapi::PepperFilePath& pepper_path) {
  FilePath file_path;
  switch(pepper_path.domain()) {
    case webkit::ppapi::PepperFilePath::DOMAIN_ABSOLUTE:
      NOTIMPLEMENTED();
      break;
    case webkit::ppapi::PepperFilePath::DOMAIN_MODULE_LOCAL:
      if (!pepper_path.path().IsAbsolute() &&
          !pepper_path.path().ReferencesParent())
        file_path = pepper_path.path();
      break;
    default:
      NOTREACHED();
      break;
  }
  return file_path;
}

}  // namespace

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
  FilePath full_path = ConvertPepperFilePath(path);
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
  FilePath from_full_path = ConvertPepperFilePath(from_path);
  FilePath to_full_path = ConvertPepperFilePath(to_path);
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
  FilePath full_path = ConvertPepperFilePath(path);
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
  FilePath full_path = ConvertPepperFilePath(path);
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
  FilePath full_path = ConvertPepperFilePath(path);
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
  FilePath full_path = ConvertPepperFilePath(path);
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
