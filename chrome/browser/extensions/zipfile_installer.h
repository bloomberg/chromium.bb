// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ZIPFILE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_ZIPFILE_INSTALLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/utility_process_host_client.h"

class ExtensionService;

namespace IPC {
class Message;
}

namespace extensions {

// ZipFileInstaller unzips an extension that is zipped up via a utility process.
// The contents are then loaded via UnpackedInstaller.
class ZipFileInstaller : public content::UtilityProcessHostClient {
 public:
  static scoped_refptr<ZipFileInstaller> Create(
      ExtensionService* extension_service);

  void LoadFromZipFile(const base::FilePath& path);

  void set_be_noisy_on_failure(bool value) { be_noisy_on_failure_ = value; }

  // UtilityProcessHostClient
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  explicit ZipFileInstaller(ExtensionService* extension_service);
  virtual ~ZipFileInstaller();

  void PrepareTempDir();
  void StartWorkOnIOThread(const base::FilePath& temp_dir);
  void ReportSuccessOnUIThread(const base::FilePath& unzipped_path);
  void ReportErrorOnUIThread(const std::string& error);

  void OnUnzipSucceeded(const base::FilePath& unzipped_path);
  void OnUnzipFailed(const std::string& error);

  bool be_noisy_on_failure_;
  base::WeakPtr<ExtensionService> extension_service_weak_;
  base::FilePath zip_path_;

  DISALLOW_COPY_AND_ASSIGN(ZipFileInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ZIPFILE_INSTALLER_H_
