// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/module_utils.h"

#include <atlbase.h>
#include "base/file_path.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/shared_memory.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"

const char kSharedMemoryName[] = "ChromeFrameVersionBeacon";
const uint32 kSharedMemorySize = 128;
const uint32 kSharedMemoryLockTimeoutMs = 1000;

// static
DllRedirector::DllRedirector() : first_module_handle_(NULL) {
  // TODO(robertshield): Correctly construct the profile name here. Also allow
  // for overrides to be taken from the environment.
  shared_memory_.reset(new base::SharedMemory(ASCIIToWide(kSharedMemoryName)));
}

DllRedirector::DllRedirector(const char* shared_memory_name)
    : shared_memory_name_(shared_memory_name), first_module_handle_(NULL) {
  // TODO(robertshield): Correctly construct the profile name here. Also allow
  // for overrides to be taken from the environment.
  shared_memory_.reset(new base::SharedMemory(ASCIIToWide(shared_memory_name)));
}

DllRedirector::~DllRedirector() {
  if (first_module_handle_) {
    if (first_module_handle_ != reinterpret_cast<HMODULE>(&__ImageBase)) {
      FreeLibrary(first_module_handle_);
    }
    first_module_handle_ = NULL;
  }
  UnregisterAsFirstCFModule();
}

bool DllRedirector::RegisterAsFirstCFModule() {
  DCHECK(first_module_handle_ == NULL);

  // Build our own file version outside of the lock:
  scoped_ptr<Version> our_version(GetCurrentModuleVersion());

  // We sadly can't use the autolock here since we want to have a timeout.
  // Be careful not to return while holding the lock. Also, attempt to do as
  // little as possible while under this lock.
  bool lock_acquired = shared_memory_->Lock(kSharedMemoryLockTimeoutMs);

  if (!lock_acquired) {
    // We couldn't get the lock in a reasonable amount of time, so fall
    // back to loading our current version. We return true to indicate that the
    // caller should not attempt to delegate to an already loaded version.
    dll_version_.swap(our_version);
    first_module_handle_ = reinterpret_cast<HMODULE>(&__ImageBase);
    return true;
  }

  bool created_beacon = true;
  bool result = shared_memory_->CreateNamed(shared_memory_name_.c_str(),
                                            false,  // open_existing
                                            kSharedMemorySize);

  if (!result) {
    created_beacon = false;

    // We failed to create the shared memory segment, suggesting it may already
    // exist: try to create it read-only.
    result = shared_memory_->Open(shared_memory_name_.c_str(),
                                  true /* read_only */);
  }

  if (result) {
    // Map in the whole thing.
    result = shared_memory_->Map(0);
    DCHECK(shared_memory_->memory());

    if (result) {
      // Either write our own version number or read it in if it was already
      // present in the shared memory section.
      if (created_beacon) {
        dll_version_.swap(our_version);

        lstrcpynA(reinterpret_cast<char*>(shared_memory_->memory()),
                  dll_version_->GetString().c_str(),
                  std::min(kSharedMemorySize,
                           dll_version_->GetString().length() + 1));

        // Mark ourself as the first module in.
        first_module_handle_ = reinterpret_cast<HMODULE>(&__ImageBase);
      } else {
        char buffer[kSharedMemorySize] = {0};
        memcpy(buffer, shared_memory_->memory(), kSharedMemorySize - 1);
        dll_version_.reset(Version::GetVersionFromString(buffer));

        if (!dll_version_.get() || dll_version_->Equals(*our_version.get())) {
          // If we either couldn't parse a valid version out of the shared
          // memory or we did parse a version and it is the same as our own,
          // then pretend we're first in to avoid trying to load any other DLLs.
          dll_version_.reset(our_version.release());
          first_module_handle_ = reinterpret_cast<HMODULE>(&__ImageBase);
          created_beacon = true;
        }
      }
    } else {
      NOTREACHED() << "Failed to map in version beacon.";
    }
  } else {
    NOTREACHED() << "Could not create file mapping for version beacon, gle: "
                 << ::GetLastError();
  }

  // Matching Unlock.
  shared_memory_->Unlock();

  return created_beacon;
}

void DllRedirector::UnregisterAsFirstCFModule() {
  if (base::SharedMemory::IsHandleValid(shared_memory_->handle())) {
    bool lock_acquired = shared_memory_->Lock(kSharedMemoryLockTimeoutMs);
    if (lock_acquired) {
      // Free our handles. The last closed handle SHOULD result in it being
      // deleted.
      shared_memory_->Close();
      shared_memory_->Unlock();
    }
  }
}

LPFNGETCLASSOBJECT DllRedirector::GetDllGetClassObjectPtr() {
  HMODULE first_module_handle = GetFirstModule();

  LPFNGETCLASSOBJECT proc_ptr = reinterpret_cast<LPFNGETCLASSOBJECT>(
      GetProcAddress(first_module_handle, "DllGetClassObject"));
  if (!proc_ptr) {
    DLOG(ERROR) << "DllRedirector: Could get address of DllGetClassObject "
                   "from first loaded module, GLE: "
                << GetLastError();
    // Oh boink, the first module we loaded was somehow bogus, make ourselves
    // the first module again.
    first_module_handle = reinterpret_cast<HMODULE>(&__ImageBase);
  }
  return proc_ptr;
}

Version* DllRedirector::GetCurrentModuleVersion() {
  scoped_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  DCHECK(file_version_info.get());

  Version* current_version = NULL;
  if (file_version_info.get()) {
     current_version = Version::GetVersionFromString(
        file_version_info->file_version());
    DCHECK(current_version);
  }

  return current_version;
}

HMODULE DllRedirector::GetFirstModule() {
  DCHECK(dll_version_.get())
      << "Error: Did you call RegisterAsFirstCFModule() first?";

  if (first_module_handle_ == NULL) {
    first_module_handle_ = LoadVersionedModule(dll_version_.get());
    if (!first_module_handle_) {
      first_module_handle_ = reinterpret_cast<HMODULE>(&__ImageBase);
    }
  }

  return first_module_handle_;
}

HMODULE DllRedirector::LoadVersionedModule(Version* version) {
  DCHECK(version);

  FilePath module_path;
  PathService::Get(base::DIR_MODULE, &module_path);
  DCHECK(!module_path.empty());

  FilePath module_name = module_path.BaseName();
  module_path = module_path.DirName()
                           .Append(ASCIIToWide(version->GetString()))
                           .Append(module_name);

  HMODULE hmodule = LoadLibrary(module_path.value().c_str());
  if (hmodule == NULL) {
    DLOG(ERROR) << "Could not load reported module version "
                << version->GetString();
  }

  return hmodule;
}

