// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_ZIP_FILE_CREATOR_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_ZIP_FILE_CREATOR_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/zip_file_creator.mojom.h"
#include "content/public/browser/utility_process_mojo_client.h"

namespace file_manager {

// ZipFileCreator creates a ZIP file from a specified list of files and
// directories under a common parent directory. This is done in a sandboxed
// utility process to protect the browser process from handling arbitrary
// input data from untrusted sources.
class ZipFileCreator : public base::RefCountedThreadSafe<ZipFileCreator> {
 public:
  typedef base::Callback<void(bool)> ResultCallback;

  // Creates a zip file from the specified list of files and directories.
  ZipFileCreator(const ResultCallback& callback,
                 const base::FilePath& src_dir,
                 const std::vector<base::FilePath>& src_relative_paths,
                 const base::FilePath& dest_file);

  // Starts creating the zip file. Must be called from the UI thread.
  // The result will be passed to |callback|. After the task is finished
  // and |callback| is run, ZipFileCreator instance is deleted.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<ZipFileCreator>;

  ~ZipFileCreator();

  // Called after the dest_file |file| is opened on the blocking pool to
  // create the zip file in it using a sandboxed utility process.
  void CreateZipFile(base::File file);

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

  // Utility process used to create the zip file.
  std::unique_ptr<
      content::UtilityProcessMojoClient<chrome::mojom::ZipFileCreator>>
      utility_process_mojo_client_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_ZIP_FILE_CREATOR_H_
