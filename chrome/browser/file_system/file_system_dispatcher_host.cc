// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_dispatcher_host.h"

#include "base/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "googleurl/src/gurl.h"

FileSystemDispatcherHost::FileSystemDispatcherHost(
    IPC::Message::Sender* sender,
    HostContentSettingsMap* host_content_settings_map)
    : message_sender_(sender),
      process_handle_(0),
      shutdown_(false),
      host_content_settings_map_(host_content_settings_map) {
  DCHECK(message_sender_);
}

void FileSystemDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!shutdown_);
  DCHECK(!process_handle_);
  DCHECK(process_handle);
  process_handle_ = process_handle;
}

void FileSystemDispatcherHost::Shutdown() {
  message_sender_ = NULL;
  shutdown_ = true;
}

bool FileSystemDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  DCHECK(!shutdown_);
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(FileSystemDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenFileSystemRequest, OnOpenFileSystem)
    // TODO(kinuko): add more.
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void FileSystemDispatcherHost::OnOpenFileSystem(
    const ViewHostMsg_OpenFileSystemRequest_Params& params) {
  string16 name;
  string16 root_path;

  // TODO(kinuko): not implemented yet.

  Send(new ViewMsg_OpenFileSystemRequest_Complete(
    params.routing_id,
    params.request_id,
    false,
    name, root_path));
}

void FileSystemDispatcherHost::Send(IPC::Message* message) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    if (!ChromeThread::PostTask(
            ChromeThread::IO, FROM_HERE,
            NewRunnableMethod(this,
                              &FileSystemDispatcherHost::Send,
                              message)))
      delete message;
    return;
  }

  if (!shutdown_ && message_sender_)
    message_sender_->Send(message);
  else
    delete message;
}
