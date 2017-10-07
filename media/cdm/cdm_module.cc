// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_module.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"

// INITIALIZE_CDM_MODULE is a macro in api/content_decryption_module.h.
// However, we need to pass it as a string to GetFunctionPointer(). The follow
// macro helps expanding it into a string.
#define STRINGIFY(X) #X
#define MAKE_STRING(X) STRINGIFY(X)

namespace media {

static CdmModule* g_cdm_module = nullptr;

// static
CdmModule* CdmModule::GetInstance() {
  // The |cdm_module| will be leaked and we will not be able to call
  // |deinitialize_cdm_module_func_|. This is fine since it's never guaranteed
  // to be called, e.g. in the fast shutdown case.
  // TODO(xhwang): Find a better ownership model to make sure |cdm_module| is
  // destructed properly whenever possible (e.g. in non-fast-shutdown case).
  if (!g_cdm_module)
    g_cdm_module = new CdmModule();

  return g_cdm_module;
}

// static
void CdmModule::ResetInstanceForTesting() {
  if (!g_cdm_module)
    return;

  delete g_cdm_module;
  g_cdm_module = nullptr;
}

CdmModule::CdmModule() {}

CdmModule::~CdmModule() {
  if (deinitialize_cdm_module_func_)
    deinitialize_cdm_module_func_();
}

CdmModule::CreateCdmFunc CdmModule::GetCreateCdmFunc() {
  if (!is_initialize_called_) {
    DCHECK(false) << __func__ << " called before CdmModule is initialized.";
    return nullptr;
  }

  return create_cdm_func_;
}

void CdmModule::Initialize(const base::FilePath& cdm_path) {
  DVLOG(2) << __func__ << ": cdm_path = " << cdm_path.value();

  DCHECK(!is_initialize_called_);
  is_initialize_called_ = true;
  cdm_path_ = cdm_path;

  // Load the CDM.
  // TODO(xhwang): Report CDM load error to UMA.
  base::NativeLibraryLoadError error;
  library_.Reset(base::LoadNativeLibrary(cdm_path, &error));
  if (!library_.is_valid()) {
    LOG(ERROR) << "CDM at " << cdm_path.value() << " could not be loaded.";
    LOG(ERROR) << "Error: " << error.ToString();
    return;
  }

  // Get function pointers.
  // TODO(xhwang): Define function names in macros to avoid typo errors.
  using InitializeCdmModuleFunc = void (*)();
  InitializeCdmModuleFunc initialize_cdm_module_func =
      reinterpret_cast<InitializeCdmModuleFunc>(
          library_.GetFunctionPointer(MAKE_STRING(INITIALIZE_CDM_MODULE)));
  deinitialize_cdm_module_func_ = reinterpret_cast<DeinitializeCdmModuleFunc>(
      library_.GetFunctionPointer("DeinitializeCdmModule"));
  create_cdm_func_ = reinterpret_cast<CreateCdmFunc>(
      library_.GetFunctionPointer("CreateCdmInstance"));

  if (!initialize_cdm_module_func || !deinitialize_cdm_module_func_ ||
      !create_cdm_func_) {
    LOG(ERROR) << "Missing entry function in CDM at " << cdm_path.value();
    deinitialize_cdm_module_func_ = nullptr;
    create_cdm_func_ = nullptr;
    library_.Release();
    return;
  }

  initialize_cdm_module_func();
}

base::FilePath CdmModule::GetCdmPath() const {
  DCHECK(is_initialize_called_);
  return cdm_path_;
}

}  // namespace media
