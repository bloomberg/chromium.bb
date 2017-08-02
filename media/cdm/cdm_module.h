// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_MODULE_H_
#define MEDIA_CDM_CDM_MODULE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_native_library.h"
#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module.h"

namespace media {

class MEDIA_EXPORT CdmModule {
 public:
  static CdmModule* GetInstance();

  // Reset the CdmModule instance so that each test have it's own instance.
  static void ResetInstanceForTesting();

  ~CdmModule();

  using CreateCdmFunc = void* (*)(int cdm_interface_version,
                                  const char* key_system,
                                  uint32_t key_system_size,
                                  GetCdmHostFunc get_cdm_host_func,
                                  void* user_data);

  CreateCdmFunc GetCreateCdmFunc(const std::string& key_system);

  void SetCdmPathForTesting(const base::FilePath& cdm_path) {
    cdm_path_ = cdm_path;
  }

 private:
  using DeinitializeCdmModuleFunc = void (*)();

  CdmModule();

  // Loads the CDM, initialize function pointers and initialize the CDM module.
  // This should only be called once.
  // TODO(xhwang): Replace the |key_system| with the path to the CDM.
  void Initialize(const std::string& key_system);

  bool is_initialize_called_ = false;
  base::FilePath cdm_path_;
  base::ScopedNativeLibrary library_;
  CreateCdmFunc create_cdm_func_ = nullptr;
  DeinitializeCdmModuleFunc deinitialize_cdm_module_func_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CdmModule);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_MODULE_H_
