// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/dll_redirector.h"

#include <aclapi.h>
#include <atlbase.h>
#include <atlsecurity.h>
#include <sddl.h>

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "chrome_frame/utils.h"

const wchar_t kSharedMemoryName[] = L"ChromeFrameVersionBeacon_";
const uint32 kSharedMemorySize = 128;
const uint32 kSharedMemoryLockTimeoutMs = 1000;

// static
DllRedirector::DllRedirector() : first_module_handle_(NULL) {
  // TODO(robertshield): Allow for overrides to be taken from the environment.
  std::wstring beacon_name(kSharedMemoryName);
  beacon_name += GetHostProcessName(false);
  shared_memory_.reset(new base::SharedMemory(beacon_name));
  shared_memory_name_ = WideToUTF8(beacon_name);
}

DllRedirector::DllRedirector(const char* shared_memory_name)
    : shared_memory_name_(shared_memory_name), first_module_handle_(NULL) {
  shared_memory_.reset(new base::SharedMemory(ASCIIToWide(shared_memory_name)));
}

DllRedirector::~DllRedirector() {
  if (first_module_handle_) {
    if (first_module_handle_ != reinterpret_cast<HMODULE>(&__ImageBase)) {
      FreeLibrary(first_module_handle_);
    } else {
      NOTREACHED() << "Error, DllRedirector attempting to free self.";
    }

    first_module_handle_ = NULL;
  }
  UnregisterAsFirstCFModule();
}

// static
DllRedirector* DllRedirector::GetInstance() {
  return Singleton<DllRedirector>::get();
}

bool DllRedirector::BuildSecurityAttributesForLock(
    CSecurityAttributes* sec_attr) {
  DCHECK(sec_attr);
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    // Don't bother with changing ACLs on pre-vista.
    return false;
  }

  bool success = false;

  // Fill out the rest of the security descriptor from the process token.
  CAccessToken token;
  if (token.GetProcessToken(TOKEN_QUERY)) {
    CSecurityDesc security_desc;
    // Set the SACL from an SDDL string that allows access to low-integrity
    // processes. See http://msdn.microsoft.com/en-us/library/bb625958.aspx.
    if (security_desc.FromString(L"S:(ML;;NW;;;LW)")) {
      CSid sid_owner;
      if (token.GetOwner(&sid_owner)) {
        security_desc.SetOwner(sid_owner);
      } else {
        NOTREACHED() << "Could not get owner.";
      }
      CSid sid_group;
      if (token.GetPrimaryGroup(&sid_group)) {
        security_desc.SetGroup(sid_group);
      } else {
        NOTREACHED() << "Could not get group.";
      }
      CDacl dacl;
      if (token.GetDefaultDacl(&dacl)) {
        // Add an access control entry mask for the current user.
        // This is what grants this user access from lower integrity levels.
        CSid sid_user;
        if (token.GetUser(&sid_user)) {
          success = dacl.AddAllowedAce(sid_user, MUTEX_ALL_ACCESS);
          security_desc.SetDacl(dacl);
          sec_attr->Set(security_desc);
        }
      }
    }
  }

  return success;
}

bool DllRedirector::SetFileMappingToReadOnly(base::SharedMemoryHandle mapping) {
  bool success = false;

  CAccessToken token;
  if (token.GetProcessToken(TOKEN_QUERY)) {
    CSid sid_user;
    if (token.GetUser(&sid_user)) {
      CDacl dacl;
      dacl.AddAllowedAce(sid_user, STANDARD_RIGHTS_READ | FILE_MAP_READ);
      success = AtlSetDacl(mapping, SE_KERNEL_OBJECT, dacl);
    }
  }

  return success;
}


bool DllRedirector::RegisterAsFirstCFModule() {
  DCHECK(first_module_handle_ == NULL);

  // Build our own file version outside of the lock:
  scoped_ptr<Version> our_version(GetCurrentModuleVersion());

  // We sadly can't use the autolock here since we want to have a timeout.
  // Be careful not to return while holding the lock. Also, attempt to do as
  // little as possible while under this lock.

  bool lock_acquired = false;
  CSecurityAttributes sec_attr;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA &&
      BuildSecurityAttributesForLock(&sec_attr)) {
    // On vista and above, we need to explicitly allow low integrity access
    // to our objects. On XP, we don't bother.
    lock_acquired = shared_memory_->Lock(kSharedMemoryLockTimeoutMs, &sec_attr);
  } else {
    lock_acquired = shared_memory_->Lock(kSharedMemoryLockTimeoutMs, NULL);
  }

  if (!lock_acquired) {
    // We couldn't get the lock in a reasonable amount of time, so fall
    // back to loading our current version. We return true to indicate that the
    // caller should not attempt to delegate to an already loaded version.
    dll_version_.swap(our_version);
    return true;
  }

  bool created_beacon = true;
  bool result = shared_memory_->CreateNamed(shared_memory_name_.c_str(),
                                            false,  // open_existing
                                            kSharedMemorySize);

  if (result) {
    // We created the beacon, now we need to mutate the security attributes
    // on the shared memory to allow read-only access and let low-integrity
    // processes open it. This will fail on FAT32 file systems.
    if (!SetFileMappingToReadOnly(shared_memory_->handle())) {
      DLOG(ERROR) << "Failed to set file mapping permissions.";
    }
  } else {
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
      } else {
        char buffer[kSharedMemorySize] = {0};
        memcpy(buffer, shared_memory_->memory(), kSharedMemorySize - 1);
        dll_version_.reset(new Version(buffer));

        if (!dll_version_->IsValid() ||
            dll_version_->Equals(*our_version.get())) {
          // If we either couldn't parse a valid version out of the shared
          // memory or we did parse a version and it is the same as our own,
          // then pretend we're first in to avoid trying to load any other DLLs.
          dll_version_.reset(our_version.release());
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
    bool lock_acquired = shared_memory_->Lock(kSharedMemoryLockTimeoutMs, NULL);
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

  LPFNGETCLASSOBJECT proc_ptr = NULL;
  if (first_module_handle) {
    proc_ptr = reinterpret_cast<LPFNGETCLASSOBJECT>(
        GetProcAddress(first_module_handle, "DllGetClassObject"));
    DPLOG_IF(ERROR, !proc_ptr) << "DllRedirector: Could not get address of "
                                  "DllGetClassObject from first loaded module.";
  }

  return proc_ptr;
}

Version* DllRedirector::GetCurrentModuleVersion() {
  scoped_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  DCHECK(file_version_info.get());

  scoped_ptr<Version> current_version;
  if (file_version_info.get()) {
     current_version.reset(
         new Version(WideToASCII(file_version_info->file_version())));
    DCHECK(current_version->IsValid());
  }

  return current_version.release();
}

HMODULE DllRedirector::GetFirstModule() {
  DCHECK(dll_version_.get())
      << "Error: Did you call RegisterAsFirstCFModule() first?";

  if (first_module_handle_ == NULL) {
    first_module_handle_ = LoadVersionedModule(dll_version_.get());
  }

  if (first_module_handle_ == reinterpret_cast<HMODULE>(&__ImageBase)) {
    NOTREACHED() << "Should not be loading own version.";
    first_module_handle_ = NULL;
  }

  return first_module_handle_;
}

HMODULE DllRedirector::LoadVersionedModule(Version* version) {
  DCHECK(version);

  HMODULE hmodule = NULL;
  wchar_t system_buffer[MAX_PATH];
  HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
  system_buffer[0] = 0;
  if (GetModuleFileName(this_module, system_buffer,
                        arraysize(system_buffer)) != 0) {
    base::FilePath module_path(system_buffer);

    // For a module located in
    // Foo\XXXXXXXXX\<module>.dll, load
    // Foo\<version>\<module>.dll:
    base::FilePath module_name = module_path.BaseName();
    module_path = module_path.DirName()
                             .DirName()
                             .Append(ASCIIToWide(version->GetString()))
                             .Append(module_name);

    hmodule = LoadLibrary(module_path.value().c_str());
    if (hmodule == NULL) {
      DPLOG(ERROR) << "Could not load reported module version "
                   << version->GetString();
    }
  } else {
    DPLOG(FATAL) << "Failed to get module file name";
  }
  return hmodule;
}
