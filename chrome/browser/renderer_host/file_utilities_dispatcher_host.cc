// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/file_utilities_dispatcher_host.h"

#include "base/file_util.h"
#include "base/platform_file.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

namespace {

void WriteFileSize(IPC::Message* reply_msg,
                   const base::PlatformFileInfo& file_info) {
  ViewHostMsg_GetFileSize::WriteReplyParams(reply_msg, file_info.size);
}

void WriteFileModificationTime(IPC::Message* reply_msg,
                               const base::PlatformFileInfo& file_info) {
  ViewHostMsg_GetFileModificationTime::WriteReplyParams(
      reply_msg, file_info.last_modified);
}

}  // namespace

FileUtilitiesDispatcherHost::FileUtilitiesDispatcherHost(
    IPC::Message::Sender* sender, int process_id)
    : message_sender_(sender),
      process_id_(process_id),
      process_handle_(0),
      shutdown_(false) {
  DCHECK(message_sender_);
}

FileUtilitiesDispatcherHost::~FileUtilitiesDispatcherHost() {
}

void FileUtilitiesDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shutdown_);
  DCHECK(!process_handle_);
  DCHECK(process_handle);
  process_handle_ = process_handle;
}

void FileUtilitiesDispatcherHost::Shutdown() {
  message_sender_ = NULL;
  shutdown_ = true;
}

bool FileUtilitiesDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(!shutdown_);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FileUtilitiesDispatcherHost, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileSize, OnGetFileSize)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileModificationTime,
                                    OnGetFileModificationTime)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenFile, OnOpenFile)
    IPC_MESSAGE_UNHANDLED((handled = false))
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FileUtilitiesDispatcherHost::OnGetFileSize(
    const FilePath& path, IPC::Message* reply_msg) {
  // Get file size only when the child process has been granted permission to
  // upload the file.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadFile(
      process_id_, path)) {
    ViewHostMsg_GetFileSize::WriteReplyParams(
        reply_msg, static_cast<int64>(-1));
    Send(reply_msg);
    return;
  }

  // Getting file size could take long time if it lives on a network share,
  // so run it on FILE thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &FileUtilitiesDispatcherHost::OnGetFileInfoOnFileThread, path,
          reply_msg, &WriteFileSize));
}

void FileUtilitiesDispatcherHost::OnGetFileModificationTime(
    const FilePath& path, IPC::Message* reply_msg) {
  // Get file modification time only when the child process has been granted
  // permission to upload the file.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadFile(
      process_id_, path)) {
    ViewHostMsg_GetFileModificationTime::WriteReplyParams(reply_msg,
                                                          base::Time());
    Send(reply_msg);
    return;
  }

  // Getting file modification time could take a long time if it lives on a
  // network share, so run it on the FILE thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &FileUtilitiesDispatcherHost::OnGetFileInfoOnFileThread,
          path, reply_msg, &WriteFileModificationTime));
}

void FileUtilitiesDispatcherHost::OnGetFileInfoOnFileThread(
    const FilePath& path,
    IPC::Message* reply_msg,
    FileInfoWriteFunc write_func) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::PlatformFileInfo file_info;
  file_info.size = 0;
  file_util::GetFileInfo(path, &file_info);

  (*write_func)(reply_msg, file_info);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &FileUtilitiesDispatcherHost::Send, reply_msg));
}

void FileUtilitiesDispatcherHost::OnOpenFile(
    const FilePath& path, int mode, IPC::Message* reply_msg) {
  // Open the file only when the child process has been granted permission to
  // upload the file.
  // TODO(jianli): Do we need separate permission to control opening the file?
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadFile(
      process_id_, path)) {
    ViewHostMsg_OpenFile::WriteReplyParams(
        reply_msg,
#if defined(OS_WIN)
        base::kInvalidPlatformFileValue
#elif defined(OS_POSIX)
        base::FileDescriptor(base::kInvalidPlatformFileValue, true)
#endif
        );
    Send(reply_msg);
    return;
  }

  // Opening the file could take a long time if it lives on a network share,
  // so run it on the FILE thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &FileUtilitiesDispatcherHost::OnOpenFileOnFileThread,
          path, mode, reply_msg));
}

void FileUtilitiesDispatcherHost::OnOpenFileOnFileThread(
    const FilePath& path, int mode, IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::PlatformFile file_handle = base::CreatePlatformFile(
      path,
      (mode == 0) ? (base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ)
                  : (base::PLATFORM_FILE_CREATE_ALWAYS |
                        base::PLATFORM_FILE_WRITE),
      NULL, NULL);

  base::PlatformFile target_file_handle;
#if defined(OS_WIN)
  // Duplicate the file handle so that the renderer process can access the file.
  if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                       process_handle_, &target_file_handle, 0, false,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    // file_handle is closed whether or not DuplicateHandle succeeds.
    target_file_handle = INVALID_HANDLE_VALUE;
  }
#else
  target_file_handle = file_handle;
#endif

  ViewHostMsg_OpenFile::WriteReplyParams(
      reply_msg,
#if defined(OS_WIN)
      target_file_handle
#elif defined(OS_POSIX)
      base::FileDescriptor(target_file_handle, true)
#endif
      );

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &FileUtilitiesDispatcherHost::Send, reply_msg));
}

void FileUtilitiesDispatcherHost::Send(IPC::Message* message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!shutdown_ && message_sender_)
    message_sender_->Send(message);
  else
    delete message;
}
