// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/cdm_host_files.h"

#include <map>
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
#include "content/common/media/cdm_host_file.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/cdm_paths.h"

#if defined(POSIX_WITH_ZYGOTE)
#include "content/common/pepper_plugin_list.h"
#include "content/public/common/pepper_plugin_info.h"
#endif

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace content {

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

#if defined(POSIX_WITH_ZYGOTE)
// From the list of registered plugins, finds all registered CDMs and fills
// |cdm_adapter_paths| with found CDM adapters paths.
void GetRegisteredCdms(std::vector<base::FilePath>* cdm_adapter_paths) {
  std::vector<PepperPluginInfo> plugins;
  ComputePepperPluginList(&plugins);
  for (const auto& plugin : plugins) {
    // CDM is not an internal plugin.
    if (plugin.is_internal)
      continue;

    if (IsCdm(plugin.path))
      cdm_adapter_paths->push_back(plugin.path);
  }
}

// A global instance used on platforms where we have to open the files in the
// Zygote process.
base::LazyInstance<std::unique_ptr<CdmHostFiles>>::DestructorAtExit
    g_cdm_host_files = LAZY_INSTANCE_INITIALIZER;
#endif

}  // namespace

CdmHostFiles::CdmHostFiles() {
  DVLOG(1) << __func__;
}

CdmHostFiles::~CdmHostFiles() {
  DVLOG(1) << __func__;
}

#if defined(POSIX_WITH_ZYGOTE)
// static
void CdmHostFiles::CreateGlobalInstance() {
  DVLOG(1) << __func__;
  DCHECK(!g_cdm_host_files.Get().get());

  std::unique_ptr<CdmHostFiles> cdm_host_files =
      base::MakeUnique<CdmHostFiles>();
  cdm_host_files->OpenFilesForAllRegisteredCdms();
  g_cdm_host_files.Get().reset(cdm_host_files.release());
}

// static
std::unique_ptr<CdmHostFiles> CdmHostFiles::TakeGlobalInstance() {
  return std::move(g_cdm_host_files.Get());
}
#endif

// static
std::unique_ptr<CdmHostFiles> CdmHostFiles::Create(
    const base::FilePath& cdm_adapter_path) {
  DVLOG(1) << __func__;
  std::unique_ptr<CdmHostFiles> cdm_host_files =
      base::MakeUnique<CdmHostFiles>();
  cdm_host_files->OpenFiles(cdm_adapter_path);
  return cdm_host_files;
}

CdmHostFiles::Status CdmHostFiles::InitVerification(
    base::NativeLibrary cdm_adapter_library,
    const base::FilePath& cdm_adapter_path) {
  DVLOG(1) << __func__;
  DCHECK(cdm_adapter_library);

  // Get function pointer exported by the CDM.
  // See media/cdm/api/content_decryption_module_ext.h.
  using InitVerificationFunc =
      bool (*)(const cdm::HostFile* cdm_host_files, uint32_t num_files);
  static const char kInitVerificationFuncName[] = "VerifyCdmHost_0";

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
  if (!scoped_cdm_library.is_valid()) {
    LOG(ERROR) << "Failed to load CDM (error: " << error.ToString() << ")";
    CloseAllFiles();
    return Status::kCdmLoadFailed;
  }
  cdm_library = scoped_cdm_library.get();
#endif

  InitVerificationFunc init_verification_func =
      reinterpret_cast<InitVerificationFunc>(
          base::GetFunctionPointerFromNativeLibrary(cdm_library,
                                                    kInitVerificationFuncName));
  if (!init_verification_func) {
    LOG(ERROR) << "Function " << kInitVerificationFuncName << " not found.";
    CloseAllFiles();
    return Status::kGetFunctionFailed;
  }

  // Fills |cdm_host_files| with common and CDM specific files for
  // |cdm_adapter_path|.
  std::vector<cdm::HostFile> cdm_host_files;
  TakePlatformFiles(cdm_adapter_path, &cdm_host_files);

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

#if defined(POSIX_WITH_ZYGOTE)
void CdmHostFiles::OpenFilesForAllRegisteredCdms() {
  std::vector<base::FilePath> cdm_adapter_paths;
  GetRegisteredCdms(&cdm_adapter_paths);
  if (cdm_adapter_paths.empty()) {
    DVLOG(1) << "No CDM registered.";
    return;
  }

  for (auto& cdm_adapter_path : cdm_adapter_paths)
    OpenCdmFiles(cdm_adapter_path);

  OpenCommonFiles();
}
#endif

void CdmHostFiles::OpenFiles(const base::FilePath& cdm_adapter_path) {
  OpenCdmFiles(cdm_adapter_path);
  OpenCommonFiles();
}

void CdmHostFiles::OpenCommonFiles() {
  DCHECK(common_files_.empty());

  std::vector<CdmHostFilePath> cdm_host_file_paths;
  GetContentClient()->AddContentDecryptionModules(nullptr,
                                                  &cdm_host_file_paths);

  for (const CdmHostFilePath& value : cdm_host_file_paths) {
    common_files_.push_back(
        CdmHostFile::Create(value.file_path, value.sig_file_path));
  }
}

void CdmHostFiles::OpenCdmFiles(const base::FilePath& cdm_adapter_path) {
  DCHECK(!cdm_adapter_path.empty());
  DCHECK(!cdm_specific_files_map_.count(cdm_adapter_path));

  std::unique_ptr<CdmHostFile> cdm_adapter_file =
      CdmHostFile::Create(cdm_adapter_path, GetSigFilePath(cdm_adapter_path));

  base::FilePath cdm_path = GetCdmPath(cdm_adapter_path);
  std::unique_ptr<CdmHostFile> cdm_file =
      CdmHostFile::Create(cdm_path, GetSigFilePath(cdm_path));

  ScopedFileVector cdm_specific_files;
  cdm_specific_files.reserve(2);
  cdm_specific_files.push_back(std::move(cdm_adapter_file));
  cdm_specific_files.push_back(std::move(cdm_file));

  cdm_specific_files_map_[cdm_adapter_path] = std::move(cdm_specific_files);
}

void CdmHostFiles::TakePlatformFiles(
    const base::FilePath& cdm_adapter_path,
    std::vector<cdm::HostFile>* cdm_host_files) {
  DCHECK(cdm_host_files->empty());

  // Populate an array of cdm::HostFile.
  for (const auto& file : common_files_)
    cdm_host_files->push_back(file->TakePlatformFile());

  // Check whether CDM specific files exist.
  const auto& iter = cdm_specific_files_map_.find(cdm_adapter_path);
  if (iter == cdm_specific_files_map_.end()) {
    NOTREACHED() << "No CDM specific files for " << cdm_adapter_path.value();
  } else {
    const ScopedFileVector& cdm_specific_files = iter->second;
    for (const auto& file : cdm_specific_files)
      cdm_host_files->push_back(file->TakePlatformFile());
  }
}

void CdmHostFiles::CloseAllFiles() {
  common_files_.clear();
  cdm_specific_files_map_.clear();
}

// Question(xhwang): Any better way to check whether a plugin is a CDM? Maybe
// when we register the plugin we can set some flag explicitly?
bool IsCdm(const base::FilePath& cdm_adapter_path) {
  return !GetCdmPath(cdm_adapter_path).empty();
}

}  // namespace content
