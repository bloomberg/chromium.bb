// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/close_handle_hook_win.h"

#include <Windows.h>
#include <psapi.h>

#include <algorithm>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/iat_patch_function.h"
#include "base/win/pe_image.h"
#include "base/win/scoped_handle.h"

namespace base {
namespace debug {

namespace {

typedef BOOL (WINAPI* CloseHandleType) (HANDLE handle);

typedef BOOL (WINAPI* DuplicateHandleType)(HANDLE source_process,
                                           HANDLE source_handle,
                                           HANDLE target_process,
                                           HANDLE* target_handle,
                                           DWORD desired_access,
                                           BOOL inherit_handle,
                                           DWORD options);

CloseHandleType g_close_function = NULL;
DuplicateHandleType g_duplicate_function = NULL;

// The entry point for CloseHandle interception. This function notifies the
// verifier about the handle that is being closed, and calls the original
// function.
BOOL WINAPI CloseHandleHook(HANDLE handle) {
  base::win::OnHandleBeingClosed(handle);
  return g_close_function(handle);
}

BOOL WINAPI DuplicateHandleHook(HANDLE source_process,
                                HANDLE source_handle,
                                HANDLE target_process,
                                HANDLE* target_handle,
                                DWORD desired_access,
                                BOOL inherit_handle,
                                DWORD options) {
  if ((options & DUPLICATE_CLOSE_SOURCE) &&
      (GetProcessId(source_process) == ::GetCurrentProcessId())) {
    base::win::OnHandleBeingClosed(source_handle);
  }

  return g_duplicate_function(source_process, source_handle, target_process,
                              target_handle, desired_access, inherit_handle,
                              options);
}

// Provides a simple way to temporarily change the protection of a memory page.
class AutoProtectMemory {
 public:
  AutoProtectMemory()
      : changed_(false), address_(NULL), bytes_(0), old_protect_(0) {}

  ~AutoProtectMemory() {
    RevertProtection();
  }

  // Grants write access to a given memory range.
  bool ChangeProtection(void* address, size_t bytes);

  // Restores the original page protection.
  void RevertProtection();

 private:
  bool changed_;
  void* address_;
  size_t bytes_;
  DWORD old_protect_;

  DISALLOW_COPY_AND_ASSIGN(AutoProtectMemory);
};

bool AutoProtectMemory::ChangeProtection(void* address, size_t bytes) {
  DCHECK(!changed_);
  DCHECK(address);

  // Change the page protection so that we can write.
  MEMORY_BASIC_INFORMATION memory_info;
  if (!VirtualQuery(address, &memory_info, sizeof(memory_info)))
    return false;

  DWORD is_executable = (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY) &
                        memory_info.Protect;

  DWORD protect = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  if (!VirtualProtect(address, bytes, protect, &old_protect_))
    return false;

  changed_ = true;
  address_ = address;
  bytes_ = bytes;
  return true;
}

void AutoProtectMemory::RevertProtection() {
  if (!changed_)
    return;

  DCHECK(address_);
  DCHECK(bytes_);

  VirtualProtect(address_, bytes_, old_protect_, &old_protect_);
  changed_ = false;
  address_ = NULL;
  bytes_ = 0;
  old_protect_ = 0;
}

// Performs an EAT interception.
bool EATPatch(HMODULE module, const char* function_name,
              void* new_function, void** old_function) {
  if (!module)
    return false;

  base::win::PEImage pe(module);
  if (!pe.VerifyMagic())
    return false;

  DWORD* eat_entry = pe.GetExportEntry(function_name);
  if (!eat_entry)
    return false;

  if (!(*old_function))
    *old_function = pe.RVAToAddr(*eat_entry);

  AutoProtectMemory memory;
  if (!memory.ChangeProtection(eat_entry, sizeof(DWORD)))
    return false;

  // Perform the patch.
#pragma warning(push)
#pragma warning(disable : 4311 4302)
  // These casts generate truncation warnings because they are 32 bit specific.
  *eat_entry = reinterpret_cast<DWORD>(new_function) -
               reinterpret_cast<DWORD>(module);
#pragma warning(pop)
  return true;
}

// Performs an IAT interception.
base::win::IATPatchFunction* IATPatch(HMODULE module, const char* function_name,
                                      void* new_function, void** old_function) {
  if (!module)
    return NULL;

  base::win::IATPatchFunction* patch = new base::win::IATPatchFunction;
  __try {
    // There is no guarantee that |module| is still loaded at this point.
    if (patch->PatchFromModule(module, "kernel32.dll", function_name,
                               new_function)) {
      delete patch;
      return NULL;
    }
  } __except((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ||
              GetExceptionCode() == EXCEPTION_GUARD_PAGE ||
              GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR) ?
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    // Leak the patch.
    return NULL;
  }

  if (!(*old_function)) {
    // Things are probably messed up if each intercepted function points to
    // a different place, but we need only one function to call.
    *old_function = patch->original_function();
  }
  return patch;
}

// Keeps track of all the hooks needed to intercept functions which could
// possibly close handles.
class HandleHooks {
 public:
  HandleHooks() {}
  ~HandleHooks() {}

  bool AddIATPatch(HMODULE module);
  bool AddEATPatch();
  bool Unpatch();

 private:
  std::vector<base::win::IATPatchFunction*> hooks_;
  DISALLOW_COPY_AND_ASSIGN(HandleHooks);
};
base::LazyInstance<HandleHooks> g_hooks = LAZY_INSTANCE_INITIALIZER;

bool HandleHooks::AddIATPatch(HMODULE module) {
  if (!module)
    return false;

  base::win::IATPatchFunction* patch = NULL;
  patch = IATPatch(module, "CloseHandle", &CloseHandleHook,
                   reinterpret_cast<void**>(&g_close_function));
  if (!patch)
    return false;
  hooks_.push_back(patch);

  patch = IATPatch(module, "DuplicateHandle", &DuplicateHandleHook,
                   reinterpret_cast<void**>(&g_duplicate_function));
  if (!patch)
    return false;
  hooks_.push_back(patch);
  return true;
}

bool HandleHooks::AddEATPatch() {
   // An attempt to restore the entry on the table at destruction is not safe.
  // An attempt to restore the entry on the table at destruction is not safe.
  return (EATPatch(GetModuleHandleA("kernel32.dll"), "CloseHandle",
                   &CloseHandleHook,
                   reinterpret_cast<void**>(&g_close_function)) &&
          EATPatch(GetModuleHandleA("kernel32.dll"), "DuplicateHandle",
                   &DuplicateHandleHook,
                   reinterpret_cast<void**>(&g_duplicate_function)));
}

bool HandleHooks::Unpatch() {
  DWORD err = NO_ERROR;
  for (std::vector<base::win::IATPatchFunction*>::iterator it = hooks_.begin();
       it != hooks_.end(); ++it) {
    err = (*it)->Unpatch();
    if (err != NO_ERROR)
      break;
    delete *it;
  }
  return (err == NO_ERROR);
}

bool PatchLoadedModules(HandleHooks* hooks) {
  const DWORD kSize = 256;
  DWORD returned;
  scoped_ptr<HMODULE[]> modules(new HMODULE[kSize]);
  if (!EnumProcessModules(GetCurrentProcess(), modules.get(),
                          kSize * sizeof(HMODULE), &returned)) {
    return false;
  }
  returned /= sizeof(HMODULE);
  returned = std::min(kSize, returned);

  bool success = false;

  for (DWORD current = 0; current < returned; current++) {
    success = hooks->AddIATPatch(modules[current]);
    if (!success)
      break;
  }

  return success;
}

}  // namespace

bool InstallHandleHooks() {
  HandleHooks* hooks = g_hooks.Pointer();

  // Performing EAT interception first is safer in the presence of other
  // threads attempting to call CloseHandle.
  return (hooks->AddEATPatch() && PatchLoadedModules(hooks));
}

void RemoveHandleHooks() {
  // We are patching all loaded modules without forcing them to stay in memory,
  // removing patches is not safe.
}

}  // namespace debug
}  // namespace base
