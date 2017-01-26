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
#include "content/common/pepper_plugin_list.h"
#include "content/public/common/pepper_plugin_info.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/cdm_paths.h"

// On systems that use the zygote process to spawn child processes, we must
// open files in the zygote process.
#if defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_MACOSX) && \
    !defined(OS_ANDROID)
#define POSIX_WITH_ZYGOTE 1
#endif

namespace base {
class FilePath;
}

namespace content {

// Manages all CDM host files.
class CdmHostFiles {
 public:
  CdmHostFiles();
  ~CdmHostFiles();

#if defined(POSIX_WITH_ZYGOTE)
  // Opens CDM host files for all registered CDMs and set the global
  // CdmHostFiles instance.  On any failure, the global instance will not be
  // set and no file will be left open.
  static void CreateGlobalInstance();

  // Takes and returns the global CdmHostFiles instance. The return value could
  // be nullptr if CreateGlobalInstance() failed.
  static std::unique_ptr<CdmHostFiles> TakeGlobalInstance();
#endif

  // Opens CDM host files for the CDM adapter at |cdm_adapter_path| and returns
  // the created CdmHostFiles instance. Returns nullptr if any of the files
  // cannot be opened, in which case no file will be left open.
  static std::unique_ptr<CdmHostFiles> Create(
      const base::FilePath& cdm_adapter_path);

  // Verifies |cdm_adapter_path| CDM files by calling the function exported
  // by the CDM. If unexpected error happens, all files will be closed.
  // Otherwise, the PlatformFiles are passed to the CDM which will close the
  // files later.
  // Only returns false if the CDM returns false (when there's an immediate
  // failure). Otherwise always returns true for backward compatibility, e.g.
  // when using an old CDM which doesn't implement the verification API.
  bool VerifyFiles(base::NativeLibrary cdm_adapter_library,
                   const base::FilePath& cdm_adapter_path);

 private:
#if defined(POSIX_WITH_ZYGOTE)
  // Opens all common files and CDM specific files for all registered CDMs.
  bool OpenFilesForAllRegisteredCdms();
#endif

  // Opens all common files and CDM specific files for the CDM adapter
  // registered at |cdm_adapter_path|.
  bool OpenFiles(const base::FilePath& cdm_adapter_path);

  // Opens common CDM host files shared by all CDMs. Upon failure, close all
  // files opened.
  bool OpenCommonFiles();

  // Opens CDM specific files for the CDM adapter registered at
  // |cdm_adapter_path|. Returns whether all CDM specific files are opened.
  // Upon failure, close all files opened.
  bool OpenCdmFiles(const base::FilePath& cdm_adapter_path);

  // Fills |cdm_host_files| with common and CDM specific files for
  // |cdm_adapter_path|. The ownership of those files are also transferred.
  // Returns true upon success where the remaining files will be closed.
  // Returns false upon any failure and all files will be closed.
  bool TakePlatformFiles(const base::FilePath& cdm_adapter_path,
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
