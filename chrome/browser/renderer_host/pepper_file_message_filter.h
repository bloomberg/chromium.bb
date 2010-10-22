// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_FILE_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_FILE_MESSAGE_FILTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "webkit/glue/plugins/pepper_dir_contents.h"

class Profile;

// A message filter for Pepper-specific File I/O messages.
//
// NOTE: Contrary to most message filters, this one handles the messages (On*
// functions) on the FILE thread instead of the IO thread: OnMessageReceived
// forwards the mesage to the FILE thread, where it's handled by
// OnMessageReceivedFileThread.
class PepperFileMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  PepperFileMessageFilter(int child_id, Profile* profile);

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual void OnChannelClosing();
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnDestruct() const;

  // Called from the FILE thread.
  void Send(IPC::Message* message);

 private:
  friend class BrowserThread;
  friend class DeleteTask<PepperFileMessageFilter>;
  virtual ~PepperFileMessageFilter();
  void OnMessageReceivedFileThread(const IPC::Message& message);
  bool SendFromIOThread(IPC::Message* message);

  // Called on the FILE thread:
  void OnPepperOpenFile(const FilePath& path,
                        int flags,
                        base::PlatformFileError* error,
                        IPC::PlatformFileForTransit* file);
  void OnPepperRenameFile(const FilePath& path_from,
                          const FilePath& path_to,
                          base::PlatformFileError* error);
  void OnPepperDeleteFileOrDir(const FilePath& path,
                               bool recursive,
                               base::PlatformFileError* error);
  void OnPepperCreateDir(const FilePath& path,
                         base::PlatformFileError* error);
  void OnPepperQueryFile(const FilePath& path,
                         base::PlatformFileInfo* info,
                         base::PlatformFileError* error);
  void OnPepperGetDirContents(const FilePath& path,
                              PepperDirContents* contents,
                              base::PlatformFileError* error);

  FilePath MakePepperPath(const FilePath& base_path);

  // The process handle for the peer.
  base::ProcessHandle handle_;

  // The channel associated with the renderer connection. This pointer is not
  // owned by this class.
  IPC::Channel* channel_;

  // The base path for the pepper data.
  FilePath pepper_path_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_FILE_MESSAGE_FILTER_H_
