// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_FILE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_FILE_MESSAGE_FILTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "base/task.h"
#include "build/build_config.h"
#include "content/browser/browser_message_filter.h"
#include "ipc/ipc_platform_file.h"
#include "webkit/plugins/ppapi/dir_contents.h"

namespace content {
class BrowserContext;
}

namespace webkit {
namespace ppapi {
class PepperFilePath;
}
}

// A message filter for Pepper-specific File I/O messages.
class PepperFileMessageFilter : public BrowserMessageFilter {
 public:
  PepperFileMessageFilter(int child_id,
                          content::BrowserContext* browser_context);

  // BrowserMessageFilter methods:
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;

  int child_id() const { return child_id_; }

 private:
  friend class content::BrowserThread;
  friend class DeleteTask<PepperFileMessageFilter>;
  virtual ~PepperFileMessageFilter();

  // Called on the FILE thread:
  void OnOpenFile(const webkit::ppapi::PepperFilePath& path,
                  int flags,
                  base::PlatformFileError* error,
                  IPC::PlatformFileForTransit* file);
  void OnRenameFile(const webkit::ppapi::PepperFilePath& from_path,
                    const webkit::ppapi::PepperFilePath& to_path,
                    base::PlatformFileError* error);
  void OnDeleteFileOrDir(const webkit::ppapi::PepperFilePath& path,
                         bool recursive,
                         base::PlatformFileError* error);
  void OnCreateDir(const webkit::ppapi::PepperFilePath& path,
                   base::PlatformFileError* error);
  void OnQueryFile(const webkit::ppapi::PepperFilePath& path,
                   base::PlatformFileInfo* info,
                   base::PlatformFileError* error);
  void OnGetDirContents(const webkit::ppapi::PepperFilePath& path,
                        webkit::ppapi::DirContents* contents,
                        base::PlatformFileError* error);

  // Validate and convert the Pepper file path to a "real" |FilePath|. Returns
  // an empty |FilePath| on error.
  FilePath ValidateAndConvertPepperFilePath(
      const webkit::ppapi::PepperFilePath& pepper_path, int flags);

  // The ID of the child process.
  const int child_id_;

  // The channel associated with the renderer connection. This pointer is not
  // owned by this class.
  IPC::Channel* channel_;

  // The base path for the pepper data.
  FilePath pepper_path_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_FILE_MESSAGE_FILTER_H_
