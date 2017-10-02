// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_CDM_HOST_FILES_H_
#define CONTENT_COMMON_MEDIA_CDM_HOST_FILES_H_

#include <map>
#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "content/common/media/cdm_host_file.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/cdm_paths.h"

namespace base {
class FilePath;
}

namespace content {

// Manages all CDM host files.
class CdmHostFiles {
 public:
  CdmHostFiles();
  ~CdmHostFiles();

  // Opens CDM host files for the CDM adapter at |cdm_adapter_path| and returns
  // the created CdmHostFiles instance. Returns nullptr if any of the files
  // cannot be opened, in which case no file will be left open.
  static std::unique_ptr<CdmHostFiles> Create(
      const base::FilePath& cdm_adapter_path);

  // Status of CDM host verification.
  // Note: Reported to UMA. Do not change the values.
  enum class Status {
    kNotCalled = 0,
    kSuccess = 1,
    kCdmLoadFailed = 2,
    kGetFunctionFailed = 3,
    kInitVerificationFailed = 4,
    kStatusCount
  };

  // Initializes the verification of CDM files by calling the function exported
  // by the CDM. If unexpected error happens, all files will be closed.
  // Otherwise, the PlatformFiles are passed to the CDM which will close the
  // files later.
  Status InitVerification(base::NativeLibrary cdm_adapter_library,
                          const base::FilePath& cdm_adapter_path);

 private:
  // Opens all common files and CDM specific files for the CDM adapter
  // registered at |cdm_adapter_path|.
  void OpenFiles(const base::FilePath& cdm_adapter_path);

  // Opens common CDM host files shared by all CDMs.
  void OpenCommonFiles();

  // Opens CDM specific files for the CDM adapter registered at
  // |cdm_adapter_path|.
  void OpenCdmFiles(const base::FilePath& cdm_adapter_path);

  // Fills |cdm_host_files| with common and CDM specific files for
  // |cdm_adapter_path|. The ownership of those files are also transferred.
  void TakePlatformFiles(const base::FilePath& cdm_adapter_path,
                         std::vector<cdm::HostFile>* cdm_host_files);

  void CloseAllFiles();

  using ScopedFileVector = std::vector<std::unique_ptr<CdmHostFile>>;
  ScopedFileVector common_files_;
  std::map<base::FilePath, ScopedFileVector> cdm_specific_files_map_;
};

// Returns whether the |cdm_adapter_path| corresponds to a known CDM.
bool IsCdm(const base::FilePath& cdm_adapter_path);

}  // namespace content

#endif  // CONTENT_COMMON_MEDIA_CDM_HOST_FILES_H_
