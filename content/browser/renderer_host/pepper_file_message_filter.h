// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/sequenced_task_runner_helpers.h"
#include "build/build_config.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/shared_impl/dir_contents.h"

namespace content {
class BrowserContext;
}

namespace ppapi {
class PepperFilePath;
}

// A message filter for Pepper-specific File I/O messages. Used on
// renderer channels, this denys the renderer the trusted operations
// permitted only by plugin processes.
class PepperFileMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit PepperFileMessageFilter(int child_id);

  // content::BrowserMessageFilter methods:
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;

  int child_id() const { return child_id_; }

  // Returns the name of the pepper data directory that we'll use for local
  // storage. The argument is the profile path so that this can be used on any
  // thread independent of the profile context.
  static FilePath GetDataDirName(const FilePath& profile_path);

 protected:
  virtual ~PepperFileMessageFilter();

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<PepperFileMessageFilter>;

  // Called on the FILE thread:
  void OnOpenFile(const ppapi::PepperFilePath& path,
                  int flags,
                  base::PlatformFileError* error,
                  IPC::PlatformFileForTransit* file);
  void OnRenameFile(const ppapi::PepperFilePath& from_path,
                    const ppapi::PepperFilePath& to_path,
                    base::PlatformFileError* error);
  void OnDeleteFileOrDir(const ppapi::PepperFilePath& path,
                         bool recursive,
                         base::PlatformFileError* error);
  void OnCreateDir(const ppapi::PepperFilePath& path,
                   base::PlatformFileError* error);
  void OnQueryFile(const ppapi::PepperFilePath& path,
                   base::PlatformFileInfo* info,
                   base::PlatformFileError* error);
  void OnGetDirContents(const ppapi::PepperFilePath& path,
                        ppapi::DirContents* contents,
                        base::PlatformFileError* error);
  void OnCreateTemporaryFile(base::PlatformFileError* error,
                             IPC::PlatformFileForTransit* file);

  // Validate and convert the Pepper file path to a "real" |FilePath|. Returns
  // an empty |FilePath| on error.
  virtual FilePath ValidateAndConvertPepperFilePath(
      const ppapi::PepperFilePath& pepper_path, int flags);

  // The ID of the child process.
  const int child_id_;

  // The channel associated with the renderer connection. This pointer is not
  // owned by this class.
  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileMessageFilter);
};

// Message filter used with out-of-process pepper flash plugin channels that
// provides the trusted operations permitted only by plugin processes.
class PepperTrustedFileMessageFilter : public PepperFileMessageFilter {
 public:
  PepperTrustedFileMessageFilter(int child_id,
                                 const std::string& plugin_name,
                                 const FilePath& profile_data_directory);

 protected:
  virtual ~PepperTrustedFileMessageFilter();

 private:
  virtual FilePath ValidateAndConvertPepperFilePath(
      const ppapi::PepperFilePath& pepper_path, int flags) OVERRIDE;

  // The path to the per-plugin directory under the per-profile data directory
  // (includes module name).
  FilePath plugin_data_directory_;

  DISALLOW_COPY_AND_ASSIGN(PepperTrustedFileMessageFilter);
};

// Message filter used with in-process pepper flash plugins that provides the
// renderer channels with the trusted operations permitted only by plugin
// process. This should not be used as part of normal operations, and may
// only be applied under the control of a command-line flag.
class PepperUnsafeFileMessageFilter : public PepperFileMessageFilter {
 public:
  PepperUnsafeFileMessageFilter(int child_id,
                                const FilePath& profile_data_directory);

 protected:
  virtual ~PepperUnsafeFileMessageFilter();

 private:
  virtual FilePath ValidateAndConvertPepperFilePath(
      const ppapi::PepperFilePath& pepper_path, int flags) OVERRIDE;

  // The per-profile data directory (not including module name).
  FilePath profile_data_directory_;

  DISALLOW_COPY_AND_ASSIGN(PepperUnsafeFileMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_FILE_MESSAGE_FILTER_H_
