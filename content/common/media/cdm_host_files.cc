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
#include "media/base/media_switches.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/cdm_paths.h"

#if defined(POSIX_WITH_ZYGOTE)
#include "content/common/pepper_plugin_list.h"
#include "content/public/common/pepper_plugin_info.h"
#endif

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace content {

namespace {

bool IgnoreMissingCdmHostFile() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kIgnoreMissingCdmHostFile);
}

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
base::LazyInstance<std::unique_ptr<CdmHostFiles>> g_cdm_host_files =
    LAZY_INSTANCE_INITIALIZER;
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
  if (!cdm_host_files->OpenFilesForAllRegisteredCdms()) {
    DVLOG(1) << __func__ << " failed.";
    cdm_host_files.reset();
    return;
  }

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
  if (!cdm_host_files->OpenFiles(cdm_adapter_path)) {
    cdm_host_files.reset();
    return nullptr;
  }

  return cdm_host_files;
}

bool CdmHostFiles::VerifyFiles(base::NativeLibrary cdm_adapter_library,
                               const base::FilePath& cdm_adapter_path) {
  DVLOG(1) << __func__;
  DCHECK(cdm_adapter_library);

  // Get function pointer exported by the CDM.
  // See media/cdm/api/content_decryption_module_ext.h.
  using VerifyCdmHostFunc =
      bool (*)(const cdm::HostFile* cdm_host_files, uint32_t num_files);
  static const char kVerifyCdmHostFuncName[] = "VerifyCdmHost_0";

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
    return true;
  }
  cdm_library = scoped_cdm_library.get();
#endif

  VerifyCdmHostFunc verify_cdm_host_func = reinterpret_cast<VerifyCdmHostFunc>(
      base::GetFunctionPointerFromNativeLibrary(cdm_library,
                                                kVerifyCdmHostFuncName));
  if (!verify_cdm_host_func) {
    LOG(ERROR) << "Function " << kVerifyCdmHostFuncName << " not found.";
    CloseAllFiles();
    return true;
  }

  // Fills |cdm_host_files| with common and CDM specific files for
  // |cdm_adapter_path|.
  std::vector<cdm::HostFile> cdm_host_files;
  if (!TakePlatformFiles(cdm_adapter_path, &cdm_host_files)) {
    DVLOG(1) << "Failed to take platform files.";
    CloseAllFiles();
    return true;
  }

  // Call |verify_cdm_host_func| on the CDM with |cdm_host_files|. Note that
  // the ownership of these files are transferred to the CDM, which will close
  // the files immediately after use.
  DVLOG(1) << __func__ << ": Calling " << kVerifyCdmHostFuncName << "().";
  if (!verify_cdm_host_func(cdm_host_files.data(), cdm_host_files.size())) {
    DVLOG(1) << "Failed to verify CDM host.";
    CloseAllFiles();
    return false;
  }

  // Close all files not passed to the CDM.
  CloseAllFiles();
  return true;
}

#if defined(POSIX_WITH_ZYGOTE)
bool CdmHostFiles::OpenFilesForAllRegisteredCdms() {
  std::vector<base::FilePath> cdm_adapter_paths;
  GetRegisteredCdms(&cdm_adapter_paths);
  if (cdm_adapter_paths.empty()) {
    DVLOG(1) << "No CDM registered.";
    return false;
  }

  // Ignore
  for (auto& cdm_adapter_path : cdm_adapter_paths) {
    bool result = OpenCdmFiles(cdm_adapter_path);
    if (!result)
      DVLOG(1) << "CDM files cannot be opened for " << cdm_adapter_path.value();
    // Ignore the failure and try other registered CDM.
  }

  if (cdm_specific_files_map_.empty()) {
    DVLOG(1) << "CDM specific files cannot be opened for any registered CDM.";
    return false;
  }

  return OpenCommonFiles();
}
#endif

bool CdmHostFiles::OpenFiles(const base::FilePath& cdm_adapter_path) {
  if (!OpenCdmFiles(cdm_adapter_path))
    return false;

  return OpenCommonFiles();
}

bool CdmHostFiles::OpenCommonFiles() {
  DCHECK(common_files_.empty());

  std::vector<CdmHostFilePath> cdm_host_file_paths;
  GetContentClient()->AddContentDecryptionModules(nullptr,
                                                  &cdm_host_file_paths);

  for (const CdmHostFilePath& value : cdm_host_file_paths) {
    std::unique_ptr<CdmHostFile> cdm_host_file =
        CdmHostFile::Create(value.file_path, value.sig_file_path);
    if (cdm_host_file) {
      common_files_.push_back(std::move(cdm_host_file));
      continue;
    }

    if (!IgnoreMissingCdmHostFile())
      return false;
  }

  return true;
}

bool CdmHostFiles::OpenCdmFiles(const base::FilePath& cdm_adapter_path) {
  DCHECK(!cdm_adapter_path.empty());
  DCHECK(!cdm_specific_files_map_.count(cdm_adapter_path));

  std::unique_ptr<CdmHostFile> cdm_adapter_file =
      CdmHostFile::Create(cdm_adapter_path, GetSigFilePath(cdm_adapter_path));
  if (!cdm_adapter_file)
    return false;

  base::FilePath cdm_path = GetCdmPath(cdm_adapter_path);
  std::unique_ptr<CdmHostFile> cdm_file =
      CdmHostFile::Create(cdm_path, GetSigFilePath(cdm_path));
  if (!cdm_file)
    return false;

  ScopedFileVector cdm_specific_files;
  cdm_specific_files.reserve(2);
  cdm_specific_files.push_back(std::move(cdm_adapter_file));
  cdm_specific_files.push_back(std::move(cdm_file));

  cdm_specific_files_map_[cdm_adapter_path] = std::move(cdm_specific_files);
  return true;
}

bool CdmHostFiles::TakePlatformFiles(
    const base::FilePath& cdm_adapter_path,
    std::vector<cdm::HostFile>* cdm_host_files) {
  DCHECK(cdm_host_files->empty());

  if (!IgnoreMissingCdmHostFile())
    DCHECK(!common_files_.empty());

  // Check whether CDM specific files exist.
  const auto& iter = cdm_specific_files_map_.find(cdm_adapter_path);
  if (iter == cdm_specific_files_map_.end()) {
    // This could happen on Linux where CDM files fail to open for Foo CDM, but
    // now we hit Bar CDM.
    DVLOG(1) << "No CDM specific files for " << cdm_adapter_path.value();
    return false;
  }

  const ScopedFileVector& cdm_specific_files = iter->second;

  cdm_host_files->reserve(common_files_.size() + cdm_specific_files.size());

  // Populate an array of cdm::HostFile.
  for (const auto& file : common_files_)
    cdm_host_files->push_back(file->TakePlatformFile());

  for (const auto& file : cdm_specific_files)
    cdm_host_files->push_back(file->TakePlatformFile());

  return true;
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
