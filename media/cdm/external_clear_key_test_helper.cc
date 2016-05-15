// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/external_clear_key_test_helper.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "media/cdm/api/content_decryption_module.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// INITIALIZE_CDM_MODULE is a macro in api/content_decryption_module.h.
// However, we need to pass it as a string to GetFunctionPointer() once it
// is expanded.
#define STRINGIFY(X) #X
#define MAKE_STRING(X) STRINGIFY(X)

// File name of the External ClearKey CDM on different platforms.
const base::FilePath::CharType kExternalClearKeyCdmFileName[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("libclearkeycdm.dylib");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("clearkeycdm.dll");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("libclearkeycdm.so");
#endif

ExternalClearKeyTestHelper::ExternalClearKeyTestHelper() {
  LoadLibrary();
}

ExternalClearKeyTestHelper::~ExternalClearKeyTestHelper() {
  UnloadLibrary();
}

void ExternalClearKeyTestHelper::LoadLibrary() {
  // Determine the location of the CDM. It is expected to be in the same
  // directory as the current module.
  base::FilePath current_module_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &current_module_dir));
  library_path_ =
      current_module_dir.Append(base::FilePath(kExternalClearKeyCdmFileName));
  ASSERT_TRUE(base::PathExists(library_path_)) << library_path_.value();

  // Now load the CDM library.
  base::NativeLibraryLoadError error;
  library_.Reset(base::LoadNativeLibrary(library_path_, &error));
  ASSERT_TRUE(library_.is_valid()) << error.ToString();

  // Call INITIALIZE_CDM_MODULE()
  typedef void (*InitializeCdmFunc)();
  InitializeCdmFunc initialize_cdm_func = reinterpret_cast<InitializeCdmFunc>(
      library_.GetFunctionPointer(MAKE_STRING(INITIALIZE_CDM_MODULE)));
  ASSERT_TRUE(initialize_cdm_func) << "No INITIALIZE_CDM_MODULE in library";
  initialize_cdm_func();
}

void ExternalClearKeyTestHelper::UnloadLibrary() {
  // Call DeinitializeCdmModule()
  typedef void (*DeinitializeCdmFunc)();
  DeinitializeCdmFunc deinitialize_cdm_func =
      reinterpret_cast<DeinitializeCdmFunc>(
          library_.GetFunctionPointer("DeinitializeCdmModule"));
  ASSERT_TRUE(deinitialize_cdm_func) << "No DeinitializeCdmModule() in library";
  deinitialize_cdm_func();
}

}  // namespace media
