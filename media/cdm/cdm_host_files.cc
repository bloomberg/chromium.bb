// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_host_files.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/cdm_paths.h"

// Needed for finding CDM path from CDM adapter path.
// TODO(crbug.com/403462): Remove this after CDM adapter is deprecated.
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace media {

namespace {

// TODO(xhwang): Move this to a common place if needed.
const base::FilePath::CharType kSignatureFileExtension[] =
    FILE_PATH_LITERAL(".sig");

// Returns the signature file path given the |file_path|. This function should
// only be used when the signature file and the file are located in the same
// directory, which is the case for the CDM and CDM adapter.
base::FilePath GetSigFilePath(const base::FilePath& file_path) {
  return file_path.AddExtension(kSignatureFileExtension);
}

// Returns the CDM library file name given the |cdm_adapter_file_name|. Returns
// nullptr if |cdm_adapter_file_name| does not correspond to a known CDM.
const char* GetCdmFileName(const base::FilePath& cdm_adapter_file_name) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (cdm_adapter_file_name ==
      base::FilePath::FromUTF8Unsafe(kWidevineCdmAdapterFileName))
    return kWidevineCdmLibraryName;
#endif

  // Clear Key CDM. For test only.
  if (cdm_adapter_file_name ==
      base::FilePath::FromUTF8Unsafe(media::kClearKeyCdmAdapterFileName))
    return media::kClearKeyCdmLibraryName;

  return nullptr;
}

// Returns the path to the CDM binary given the |cdm_adapter_path|. Returns an
// empty path if |cdm_adapter_path| does not correspond to a known CDM.
base::FilePath GetCdmPath(const base::FilePath& cdm_adapter_path) {
  const char* cdm_file_name = GetCdmFileName(cdm_adapter_path.BaseName());
  if (!cdm_file_name)
    return base::FilePath();

  return cdm_adapter_path.DirName().AppendASCII(
      base::GetNativeLibraryName(cdm_file_name));
}

// Gets the library where InitVerificationFunc pointer can be obtained.
// TODO(crbug.com/403462): Remove this after CDM adapter is deprecated.
base::NativeLibrary GetCdmLibrary(base::NativeLibrary cdm_adapter_library,
                                  const base::FilePath& cdm_adapter_path) {
  DCHECK(cdm_adapter_library);

  base::NativeLibrary cdm_library;

#if defined(OS_LINUX) || defined(OS_MACOSX)
  // On POSIX, "the dlsym() function shall search for the named symbol in all
  // objects loaded automatically as a result of loading the object referenced
  // by handle". Since the CDM is loaded automatically as a result of loading
  // the CDM adapter, we can just use the adapter to look for CDM symbols.
  cdm_library = cdm_adapter_library;
#elif defined(OS_WIN)
  // On Windows, we have manually load the CDM.
  base::ScopedNativeLibrary scoped_cdm_library;
  base::NativeLibraryLoadError error;
  scoped_cdm_library.Reset(
      base::LoadNativeLibrary(GetCdmPath(cdm_adapter_path), &error));
  if (!scoped_cdm_library.is_valid())
    LOG(ERROR) << "Failed to load CDM (error: " << error.ToString() << ")";
  else
    cdm_library = scoped_cdm_library.get();
#endif

  return cdm_library;
}

}  // namespace

CdmHostFiles::CdmHostFiles() {
  DVLOG(1) << __func__;
}

CdmHostFiles::~CdmHostFiles() {
  DVLOG(1) << __func__;
}

void CdmHostFiles::InitializeWithAdapter(
    const base::FilePath& cdm_adapter_path,
    const std::vector<CdmHostFilePath>& cdm_host_file_paths) {
  OpenCdmFileWithAdapter(cdm_adapter_path);
  OpenCommonFiles(cdm_host_file_paths);
}

void CdmHostFiles::Initialize(
    const base::FilePath& cdm_path,
    const std::vector<CdmHostFilePath>& cdm_host_file_paths) {
  OpenCdmFile(cdm_path);
  OpenCommonFiles(cdm_host_file_paths);
}

CdmHostFiles::Status CdmHostFiles::InitVerificationWithAdapter(
    base::NativeLibrary cdm_adapter_library,
    const base::FilePath& cdm_adapter_path) {
  DVLOG(1) << __func__;
  DCHECK(cdm_adapter_library);
  base::NativeLibrary cdm_library =
      GetCdmLibrary(cdm_adapter_library, cdm_adapter_path);
  if (!cdm_library)
    return Status::kCdmLoadFailed;

  return InitVerification(cdm_library);
}

CdmHostFiles::Status CdmHostFiles::InitVerification(
    base::NativeLibrary cdm_library) {
  DVLOG(1) << __func__;
  DCHECK(cdm_library);

  // Get function pointer exported by the CDM.
  // See media/cdm/api/content_decryption_module_ext.h.
  using InitVerificationFunc =
      bool (*)(const cdm::HostFile* cdm_host_files, uint32_t num_files);
  static const char kInitVerificationFuncName[] = "VerifyCdmHost_0";

  InitVerificationFunc init_verification_func =
      reinterpret_cast<InitVerificationFunc>(
          base::GetFunctionPointerFromNativeLibrary(cdm_library,
                                                    kInitVerificationFuncName));
  if (!init_verification_func) {
    LOG(ERROR) << "Function " << kInitVerificationFuncName << " not found.";
    CloseAllFiles();
    return Status::kGetFunctionFailed;
  }

  // Fills |cdm_host_files| with common and CDM specific files.
  std::vector<cdm::HostFile> cdm_host_files;
  TakePlatformFiles(&cdm_host_files);

  // std::vector::data() is not guaranteed to be nullptr when empty().
  const cdm::HostFile* cdm_host_files_ptr =
      cdm_host_files.empty() ? nullptr : cdm_host_files.data();

  // Call |init_verification_func| on the CDM with |cdm_host_files|. Note that
  // the ownership of these files are transferred to the CDM, which will close
  // the files immediately after use.
  DVLOG(1) << __func__ << ": Calling " << kInitVerificationFuncName
           << "() with " << cdm_host_files.size() << " files.";
  for (const auto& host_file : cdm_host_files) {
    DVLOG(1) << " - File Path: " << host_file.file_path;
    DVLOG(1) << " - File: " << host_file.file;
    DVLOG(1) << " - Sig File: " << host_file.sig_file;
  }

  if (!init_verification_func(cdm_host_files_ptr, cdm_host_files.size())) {
    DVLOG(1) << "Failed to verify CDM host.";
    CloseAllFiles();
    return Status::kInitVerificationFailed;
  }

  // Close all files not passed to the CDM.
  CloseAllFiles();
  return Status::kSuccess;
}

void CdmHostFiles::CloseAllFiles() {
  common_files_.clear();
  cdm_specific_files_.clear();
}

void CdmHostFiles::OpenCommonFiles(
    const std::vector<CdmHostFilePath>& cdm_host_file_paths) {
  DCHECK(common_files_.empty());

  for (const auto& value : cdm_host_file_paths) {
    common_files_.push_back(
        CdmHostFile::Create(value.file_path, value.sig_file_path));
  }
}

void CdmHostFiles::OpenCdmFileWithAdapter(
    const base::FilePath& cdm_adapter_path) {
  DCHECK(!cdm_adapter_path.empty());
  cdm_specific_files_.push_back(
      CdmHostFile::Create(cdm_adapter_path, GetSigFilePath(cdm_adapter_path)));

  base::FilePath cdm_path = GetCdmPath(cdm_adapter_path);
  if (cdm_path.empty()) {
    LOG(ERROR) << "CDM path is empty.";
    return;
  }

  OpenCdmFile(cdm_path);
}

void CdmHostFiles::OpenCdmFile(const base::FilePath& cdm_path) {
  DCHECK(!cdm_path.empty());
  cdm_specific_files_.push_back(
      CdmHostFile::Create(cdm_path, GetSigFilePath(cdm_path)));
}

void CdmHostFiles::TakePlatformFiles(
    std::vector<cdm::HostFile>* cdm_host_files) {
  DCHECK(cdm_host_files->empty());

  // Populate an array of cdm::HostFile.
  for (const auto& file : common_files_)
    cdm_host_files->push_back(file->TakePlatformFile());
  for (const auto& file : cdm_specific_files_)
    cdm_host_files->push_back(file->TakePlatformFile());
}

// Question(xhwang): Any better way to check whether a plugin is a CDM? Maybe
// when we register the plugin we can set some flag explicitly?
bool IsCdm(const base::FilePath& cdm_adapter_path) {
  return !GetCdmPath(cdm_adapter_path).empty();
}

}  // namespace media
