// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/self_reg_work_item.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/installer/util/logging_installer.h"

// Default registration export names.
const char kDefaultRegistrationEntryPoint[] = "DllRegisterServer";
const char kDefaultUnregistrationEntryPoint[] = "DllUnregisterServer";

// User-level registration export names.
const char kUserRegistrationEntryPoint[] = "DllRegisterUserServer";
const char kUserUnregistrationEntryPoint[] = "DllUnregisterUserServer";

SelfRegWorkItem::SelfRegWorkItem(const std::wstring& dll_path,
                                 bool do_register,
                                 bool user_level_registration)
    : do_register_(do_register), dll_path_(dll_path),
      user_level_registration_(user_level_registration) {
}

SelfRegWorkItem::~SelfRegWorkItem() {
}

bool SelfRegWorkItem::RegisterDll(bool do_register) {
  VLOG(1) << "COM " << (do_register ? "registration of " : "unregistration of ")
          << dll_path_;

  HMODULE dll_module = ::LoadLibraryEx(dll_path_.c_str(), NULL,
                                       LOAD_WITH_ALTERED_SEARCH_PATH);
  bool success = false;
  if (NULL != dll_module) {
    typedef HRESULT (WINAPI* RegisterFunc)();
    RegisterFunc register_server_func = NULL;
    if (do_register) {
      register_server_func = reinterpret_cast<RegisterFunc>(
          ::GetProcAddress(dll_module, user_level_registration_ ?
              kUserRegistrationEntryPoint : kDefaultRegistrationEntryPoint));
    } else {
      register_server_func = reinterpret_cast<RegisterFunc>(
          ::GetProcAddress(dll_module, user_level_registration_ ?
              kUserUnregistrationEntryPoint :
              kDefaultUnregistrationEntryPoint));
    }

    if (NULL != register_server_func) {
      HRESULT hr = register_server_func();
      success = SUCCEEDED(hr);
      if (!success) {
        PLOG(ERROR) << "Failed to " << (do_register ? "register" : "unregister")
                    << " DLL at " << dll_path_.c_str() <<
                    base::StringPrintf(" 0x%08X", hr);
      }
    } else {
      LOG(ERROR) << "COM registration export function not found";
    }
    ::FreeLibrary(dll_module);
  } else {
    PLOG(WARNING) << "Failed to load: " << dll_path_;
  }
  return success;
}

bool SelfRegWorkItem::Do() {
  bool success = RegisterDll(do_register_);
  if (ignore_failure_)
    success = true;
  return success;
}

void SelfRegWorkItem::Rollback() {
  if (!ignore_failure_) {
    RegisterDll(!do_register_);
  }
}
