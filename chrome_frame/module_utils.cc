// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/module_utils.h"

#include <atlbase.h>
#include <TlHelp32.h>

#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "base/version.h"

DllRedirector::DllRedirector() : dcgo_ptr_(NULL), initialized_(false),
                                 module_handle_(NULL) {}

DllRedirector::~DllRedirector() {
  if (module_handle_) {
    FreeLibrary(module_handle_);
    module_handle_ = NULL;
  }
}

void DllRedirector::EnsureInitialized(const wchar_t* module_name,
                                      REFCLSID clsid) {
  if (!initialized_) {
    initialized_ = true;
    // Also sets module_handle_.
    dcgo_ptr_ = GetDllGetClassObjectFromModuleName(module_name, clsid);
  }
}

LPFNGETCLASSOBJECT DllRedirector::get_dll_get_class_object_ptr() const {
  DCHECK(initialized_);
  return dcgo_ptr_;
}

LPFNGETCLASSOBJECT DllRedirector::GetDllGetClassObjectFromModuleName(
    const wchar_t* module_name, REFCLSID clsid) {
  module_handle_ = NULL;
  LPFNGETCLASSOBJECT proc_ptr = NULL;
  HMODULE module_handle;
  if (GetOldestNamedModuleHandle(module_name, clsid, &module_handle)) {
    HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
    if (module_handle != this_module) {
      proc_ptr = GetDllGetClassObjectPtr(module_handle);
      if (proc_ptr) {
        // Stash away the module handle in module_handle_ so that it will be
        // automatically closed when we get destroyed. GetDllGetClassObjectPtr
        // above will have incremented the module's ref count.
        module_handle_ = module_handle;
      }
    } else {
      LOG(INFO) << "Module Scan: DllGetClassObject found in current module.";
    }
  }

  return proc_ptr;
}

bool DllRedirector::GetOldestNamedModuleHandle(const std::wstring& module_name,
                                               REFCLSID clsid,
                                               HMODULE* oldest_module_handle) {
  DCHECK(oldest_module_handle);

  ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0));
  if (snapshot == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Could not create module snapshot!";
    return false;
  }

  bool success = false;
  PathToHModuleMap map;

  // First get the list of module paths, and save the full path to base address
  // mapping.
  MODULEENTRY32W module_entry;
  module_entry.dwSize = sizeof(module_entry);
  BOOL cont = Module32FirstW(snapshot, &module_entry);
  while (cont) {
    if (!lstrcmpi(module_entry.szModule, module_name.c_str())) {
      std::wstring full_path(module_entry.szExePath);
      map[full_path] = module_entry.hModule;
    }
    cont = Module32NextW(snapshot, &module_entry);
  }

  // Next, enumerate the map and find the oldest version of the module.
  // (check if the map is of size 1 first)
  if (!map.empty()) {
    if (map.size() == 1) {
      *oldest_module_handle = map.begin()->second;
    } else {
      *oldest_module_handle = GetHandleOfOldestModule(map, clsid);
    }

    if (*oldest_module_handle != NULL) {
      success = true;
    }
  } else {
    LOG(INFO) << "Module Scan: No modules named " << module_name
              << " were found.";
  }

  return success;
}

HMODULE DllRedirector::GetHandleOfOldestModule(const PathToHModuleMap& map,
                                               REFCLSID clsid) {
  HMODULE oldest_module = NULL;
  scoped_ptr<Version> min_version(
      Version::GetVersionFromString("999.999.999.999"));

  PathToHModuleMap::const_iterator map_iter(map.begin());
  for (; map_iter != map.end(); ++map_iter) {
    // First check that either we are in the current module or that the DLL
    // returns a class factory for our clsid.
    bool current_module =
        (map_iter->second == reinterpret_cast<HMODULE>(&__ImageBase));
    bool gco_succeeded = false;
    if (!current_module) {
      LPFNGETCLASSOBJECT dgco_ptr = GetDllGetClassObjectPtr(map_iter->second);
      if (dgco_ptr) {
        {
          CComPtr<IClassFactory> class_factory;
          HRESULT hr = dgco_ptr(clsid, IID_IClassFactory,
                                reinterpret_cast<void**>(&class_factory));
          gco_succeeded = SUCCEEDED(hr) && class_factory != NULL;
        }
        // Release the module ref count we picked up in GetDllGetClassObjectPtr.
        FreeLibrary(map_iter->second);
      }
    }

    if (current_module || gco_succeeded) {
      // Then check that the version is less than we've already found:
      scoped_ptr<FileVersionInfo> version_info(
          FileVersionInfo::CreateFileVersionInfo(map_iter->first));
      scoped_ptr<Version> version(
          Version::GetVersionFromString(version_info->file_version()));
      if (version->CompareTo(*min_version.get()) < 0) {
        oldest_module = map_iter->second;
        min_version.reset(version.release());
      }
    }
  }

  return oldest_module;
}

LPFNGETCLASSOBJECT DllRedirector::GetDllGetClassObjectPtr(HMODULE module) {
  LPFNGETCLASSOBJECT proc_ptr = NULL;
  HMODULE temp_handle = 0;
  // Increment the module ref count while we have an pointer to its
  // DllGetClassObject function.
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                        reinterpret_cast<LPCTSTR>(module),
                        &temp_handle)) {
    proc_ptr = reinterpret_cast<LPFNGETCLASSOBJECT>(
        GetProcAddress(temp_handle, "DllGetClassObject"));
    if (!proc_ptr) {
      FreeLibrary(temp_handle);
      LOG(ERROR) << "Module Scan: Couldn't get address of "
                 << "DllGetClassObject: "
                 << GetLastError();
    }
  } else {
    LOG(ERROR) << "Module Scan: Could not increment module count: "
               << GetLastError();
  }
  return proc_ptr;
}
