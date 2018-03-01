// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_
#define EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/public/browser/utility_process_mojo_client.h"

namespace extensions {

namespace mojom {
class ExtensionUnpacker;
}

// ZipFileInstaller unzips an extension in a utility process.
// This class is not thread-safe: it is bound to the sequence it is created on.
class ZipFileInstaller : public base::RefCountedThreadSafe<ZipFileInstaller> {
 public:
  // The callback invoked when the ZIP file installation is finished.
  // On success, |unzip_dir| points to the directory the ZIP file was installed
  // and |error| is empty. On failure, |unzip_dir| is empty and |error| contains
  // an error message describing the failure.
  using DoneCallback = base::OnceCallback<void(const base::FilePath& zip_file,
                                               const base::FilePath& unzip_dir,
                                               const std::string& error)>;

  // Creates a ZipFileInstaller that invokes |done_callback| when done.
  static scoped_refptr<ZipFileInstaller> Create(DoneCallback done_callback);

  void LoadFromZipFile(const base::FilePath& zip_file);

 private:
  friend class base::RefCountedThreadSafe<ZipFileInstaller>;

  explicit ZipFileInstaller(DoneCallback done_callback);
  ~ZipFileInstaller();

  // Unzip an extension into |unzip_dir| and load it with an UnpackedInstaller.
  void Unzip(base::Optional<base::FilePath> unzip_dir);
  void UnzipDone(const base::FilePath& unzip_dir, bool success);

  // On failure, report the |error| reason.
  void ReportFailure(const std::string& error);

  // Callback invoked when unzipping has finished.
  DoneCallback done_callback_;

  // File containing the extension to unzip.
  base::FilePath zip_file_;

  // Utility process used to perform the unzip.
  std::unique_ptr<content::UtilityProcessMojoClient<mojom::ExtensionUnpacker>>
      utility_process_mojo_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ZipFileInstaller);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_
