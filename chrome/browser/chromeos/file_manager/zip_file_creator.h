// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_ZIP_FILE_CREATOR_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_ZIP_FILE_CREATOR_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "content/public/browser/utility_process_host_client.h"

namespace file_manager {

// ZipFileCreator creates a ZIP file from a specified list of files and
// directories under a common parent directory. This is done in a sandboxed
// subprocess to protect the browser process from handling arbitrary input data
// from untrusted sources.
//
// The class is ref-counted and its ownership is passed around internal callback
// objects and finally to UtilityProcessHost. After the job finishes, the host
// releases the ref-pointer and then ZipFileCreator is automatically deleted.
class ZipFileCreator : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void(bool)> ResultCallback;

  // Creates a zip file from the specified list of files and directories.
  ZipFileCreator(const ResultCallback& callback,
                 const base::FilePath& src_dir,
                 const std::vector<base::FilePath>& src_relative_paths,
                 const base::FilePath& dest_file);

  // Starts creating the zip file. Must be called from the UI thread.
  // The result will be passed to |callback|. After the task is finished and
  // |callback| is run, ZipFileCreator instance is deleted.
  void Start();

 private:
  friend class ProcessHostClient;

  virtual ~ZipFileCreator();

  // Called after the file handle is opened on blocking pool.
  void OnOpenFileHandle(base::File file);

  // Starts the utility process that creates the zip file.
  void StartProcessOnIOThread(base::File dest_file);

  // UtilityProcessHostClient
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  // IPC message handlers.
  void OnCreateZipFileSucceeded();
  void OnCreateZipFileFailed();

  void ReportDone(bool success);

  // The callback.
  ResultCallback callback_;

  // The source directory for input files.
  base::FilePath src_dir_;

  // The list of source files paths to be included in the zip file.
  // Entries are relative paths under directory |src_dir_|.
  std::vector<base::FilePath> src_relative_paths_;

  // The output zip file.
  base::FilePath dest_file_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_ZIP_FILE_CREATOR_H_
