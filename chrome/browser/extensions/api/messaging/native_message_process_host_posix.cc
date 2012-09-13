// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/process_util.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

void NativeMessageProcessHost::ReadNowForTesting() {
  OnFileCanReadWithoutBlocking(read_file_);
}

void NativeMessageProcessHost::InitIO() {
  // Always watch the read end.
  MessageLoopForIO::current()->WatchFileDescriptor(read_file_,
                                                   true, /* persistent */
                                                   MessageLoopForIO::WATCH_READ,
                                                   &read_watcher_,
                                                   this);
}

void NativeMessageProcessHost::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Make sure that the fd given to us is the same one we started with.
  CHECK_EQ(fd, read_file_);

  // If this is a sendMessage request, stop trying to read after the first
  // message.
  if (is_send_message_)
    read_watcher_.StopWatchingFileDescriptor();

  MessageType type;
  std::string message;
  if (!ReadMessage(&type, &message)) {
    // A read failed, is the process dead?
    if (base::GetTerminationStatus(native_process_handle_, NULL) !=
        base::TERMINATION_STATUS_STILL_RUNNING) {
      read_watcher_.StopWatchingFileDescriptor();
      // Notify the message service that the channel should close.
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
          base::Bind(&Client::CloseChannel, weak_client_ui_,
                     destination_port_, true));
    }
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&Client::PostMessageFromNativeProcess, weak_client_ui_,
                 destination_port_, message));
}

bool NativeMessageProcessHost::WriteData(FileHandle file,
                                         const char* data,
                                         size_t bytes_to_write) {
  return file_util::WriteFileDescriptor(file, data, bytes_to_write);
}

bool NativeMessageProcessHost::ReadData(FileHandle file,
                                        char* data,
                                        size_t bytes_to_read) {
  return file_util::ReadFromFD(file, data, bytes_to_read);
}

}  // namespace extensions
