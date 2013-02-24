// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ZIP_FILE_CREATOR_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ZIP_FILE_CREATOR_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host_client.h"

namespace extensions {

// ZipFileCreator creates a ZIP file from a specified list of files and
// directories under a common parent directory. This is done in a sandboxed
// subprocess to protect the browser process from handling arbitrary input data
// from untrusted sources.
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessHost.
//
// Additionally, we hold a reference to our own client so that it lives at least
// long enough to receive the result of zip file creation.
class ZipFileCreator : public content::UtilityProcessHostClient {
 public:
  class Observer {
   public:
    virtual void OnZipDone(bool success) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Creates a zip file from the specified list of files and directories.
  ZipFileCreator(Observer* observer,
                 const base::FilePath& src_dir,
                 const std::vector<base::FilePath>& src_relative_paths,
                 const base::FilePath& dest_file);

  // Start creating the zip file. The client is called with the results.
  void Start();

 private:
  class ProcessHostClient;
  friend class ProcessHostClient;

  virtual ~ZipFileCreator();

  // Opens a handle for the zip file, and proceeds to StartProcessOnIOThread.
  void OpenFileHandleOnBlockingThreadPool();

  // Starts the utility process that creates the zip file.
  void StartProcessOnIOThread(base::PlatformFile dest_file);

  // UtilityProcessHostClient
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;

  // IPC message handlers.
  void OnCreateZipFileSucceeded();
  void OnCreateZipFileFailed();

  void ReportDone(bool success);

  // The observer's thread. This is the thread we respond on.
  content::BrowserThread::ID thread_identifier_;

  // The observer.
  Observer* observer_;

  // The source directory for input files.
  base::FilePath src_dir_;

  // The list of source files paths to be included in the zip file.
  // Entries are relative paths under directory |src_dir_|.
  std::vector<base::FilePath> src_relative_paths_;

  // The output zip file.
  base::FilePath dest_file_;

  // Whether we've received a response from the utility process yet.
  bool got_response_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ZIP_FILE_CREATOR_H_
