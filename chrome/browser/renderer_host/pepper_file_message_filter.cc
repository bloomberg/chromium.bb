// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper_file_message_filter.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/file_path.h"
#include "base/process_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_platform_file.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

PepperFileMessageFilter::PepperFileMessageFilter(
    int child_id, Profile* profile)
    : handle_(base::kNullProcessHandle),
      channel_(NULL) {
  pepper_path_ = profile->GetPath().Append(FILE_PATH_LITERAL("Pepper Data"));
}

PepperFileMessageFilter::~PepperFileMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (handle_)
    base::CloseProcessHandle(handle_);
}

// Called on the IPC thread:
void PepperFileMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

// Called on the IPC thread:
void PepperFileMessageFilter::OnChannelConnected(int32 peer_pid) {
  DCHECK(!handle_) << " " << handle_;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!base::OpenProcessHandle(peer_pid, &handle_)) {
    NOTREACHED();
  }
}

void PepperFileMessageFilter::OnChannelError() {
}

// Called on the IPC thread:
void PepperFileMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

// Called on the IPC thread:
bool PepperFileMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  switch (msg.type()) {
    case ViewHostMsg_PepperOpenFile::ID:
    case ViewHostMsg_PepperRenameFile::ID:
    case ViewHostMsg_PepperDeleteFileOrDir::ID:
    case ViewHostMsg_PepperCreateDir::ID:
    case ViewHostMsg_PepperQueryFile::ID:
    case ViewHostMsg_PepperGetDirContents::ID:
      break;
    default:
      return false;
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &PepperFileMessageFilter::OnMessageReceivedFileThread, msg));

  return true;
}

void PepperFileMessageFilter::OnMessageReceivedFileThread(
    const IPC::Message& msg) {
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperFileMessageFilter, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PepperOpenFile, OnPepperOpenFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PepperRenameFile, OnPepperRenameFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PepperDeleteFileOrDir,
                        OnPepperDeleteFileOrDir)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PepperCreateDir, OnPepperCreateDir)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PepperQueryFile, OnPepperQueryFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PepperGetDirContents,
                        OnPepperGetDirContents)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(
            &BrowserRenderProcessHost::BadMessageTerminateProcess,
            msg.type(), handle_));
  }
}

void PepperFileMessageFilter::OnDestruct() {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

// Called on the FILE thread:
void PepperFileMessageFilter::Send(IPC::Message* message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
        this, &PepperFileMessageFilter::SendFromIOThread, message));
}

// Called on the IPC thread:
bool PepperFileMessageFilter::SendFromIOThread(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
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
                       handle_, file, 0, false,
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
    PepperDirContents* contents,
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
    PepperDirEntry entry = {
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
