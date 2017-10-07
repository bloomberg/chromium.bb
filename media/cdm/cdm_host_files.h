// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_HOST_FILES_H_
#define MEDIA_CDM_CDM_HOST_FILES_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/cdm_host_file.h"
#include "media/cdm/cdm_paths.h"

namespace base {
class FilePath;
}

namespace media {

// Manages all CDM host files.

// TODO(xhwang): Remove all functions suffixed with "WithAdapter" after CDM
// adapter is deprecated.

class MEDIA_EXPORT CdmHostFiles {
 public:
  CdmHostFiles();
  ~CdmHostFiles();

  // Opens all common files and CDM specific files for the CDM adapter
  // registered at |cdm_adapter_path|.
  void InitializeWithAdapter(
      const base::FilePath& cdm_adapter_path,
      const std::vector<CdmHostFilePath>& cdm_host_file_paths);

  // Opens all common files and CDM specific files for the CDM at |cdm_path|.
  void Initialize(const base::FilePath& cdm_path,
                  const std::vector<CdmHostFilePath>& cdm_host_file_paths);

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
  // NOTE: Initialize*() must be called before calling this.
  Status InitVerificationWithAdapter(base::NativeLibrary cdm_adapter_library,
                                     const base::FilePath& cdm_adapter_path);
  Status InitVerification(base::NativeLibrary cdm_library);

  void CloseAllFiles();

 private:
  // Opens common CDM host files shared by all CDMs.
  void OpenCommonFiles(const std::vector<CdmHostFilePath>& cdm_host_file_paths);

  // Opens the CDM file and the CDM adapter file.
  void OpenCdmFileWithAdapter(const base::FilePath& cdm_adapter_path);

  // Opens the CDM file.
  void OpenCdmFile(const base::FilePath& cdm_path);

  // Fills |cdm_host_files| with common and CDM specific files. The ownership
  // of those files are also transferred.
  void TakePlatformFiles(std::vector<cdm::HostFile>* cdm_host_files);

  using ScopedFileVector = std::vector<std::unique_ptr<CdmHostFile>>;

  // Files common to all CDM types, e.g. main executable.
  ScopedFileVector common_files_;

  // Files specific to each CDM type. When the CDM is hosted by a CDM adapter,
  // this includes both the CDM adapter and the CDM. Otherwise, this only
  // includes the CDM.
  ScopedFileVector cdm_specific_files_;

  DISALLOW_COPY_AND_ASSIGN(CdmHostFiles);
};

// Returns whether the |cdm_adapter_path| corresponds to a known CDM.
bool MEDIA_EXPORT IsCdm(const base::FilePath& cdm_adapter_path);

}  // namespace media

#endif  // MEDIA_CDM_CDM_HOST_FILES_H_
