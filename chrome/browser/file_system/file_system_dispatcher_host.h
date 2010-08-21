// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/nullable_string16.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "chrome/common/render_messages.h"

class HostContentSettingsMap;
class Receiver;
class ResourceMessageFilter;

class FileSystemDispatcherHost
    : public base::RefCountedThreadSafe<FileSystemDispatcherHost> {
 public:
  FileSystemDispatcherHost(IPC::Message::Sender* sender,
                           HostContentSettingsMap* host_content_settings_map);
  void Init(base::ProcessHandle process_handle);
  void Shutdown();

  bool OnMessageReceived(const IPC::Message& message, bool* message_was_ok);

  void OnOpenFileSystem(const ViewHostMsg_OpenFileSystemRequest_Params&);
  // TODO(kinuko): add more methods.

  void Send(IPC::Message* message);

 private:
  // The sender to be used for sending out IPC messages.
  IPC::Message::Sender* message_sender_;

  // The handle of this process.
  base::ProcessHandle process_handle_;

  bool shutdown_;

  // Used to look up permissions.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_
