// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ZIPFILE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_ZIPFILE_INSTALLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/utility_process_mojo_client.h"

class ExtensionService;

namespace extensions {

namespace mojom {
class ExtensionUnpacker;
}

// ZipFileInstaller unzips an extension in a utility process. On success, the
// extension content is loaded by an extensions::UnpackedInstaller. The class
// lives on the UI thread.
class ZipFileInstaller : public base::RefCountedThreadSafe<ZipFileInstaller> {
 public:
  static scoped_refptr<ZipFileInstaller> Create(ExtensionService* service);

  void LoadFromZipFile(const base::FilePath& zip_file);

  void set_be_noisy_on_failure(bool value) { be_noisy_on_failure_ = value; }

 private:
  friend class base::RefCountedThreadSafe<ZipFileInstaller>;

  explicit ZipFileInstaller(ExtensionService* service);
  ~ZipFileInstaller();

  // Unzip an extension into |unzip_dir| and load it with an UnpackedInstaller.
  void PrepareUnzipDir(const base::FilePath& zip_file);
  void Unzip(const base::FilePath& unzip_dir);
  void UnzipDone(const base::FilePath& unzip_dir, bool success);

  // On failure, report the |error| reason.
  void ReportFailure(const std::string& error);

  // Passed to the ExtensionErrorReporter when reporting errors.
  bool be_noisy_on_failure_;

  // Pointer to our controlling extension service.
  base::WeakPtr<ExtensionService> extension_service_weak_;

  // File containing the extension to unzip.
  base::FilePath zip_file_;

  // Utility process used to perform the unzip.
  std::unique_ptr<content::UtilityProcessMojoClient<mojom::ExtensionUnpacker>>
      utility_process_mojo_client_;

  DISALLOW_COPY_AND_ASSIGN(ZipFileInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ZIPFILE_INSTALLER_H_
