// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/self_reg_work_item.h"

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
  HMODULE dll_module = ::LoadLibraryEx(dll_path_.c_str(), NULL,
                                       LOAD_WITH_ALTERED_SEARCH_PATH);
  bool success = false;
  if (NULL != dll_module) {
    PROC register_server_func = NULL;
    if (do_register) {
      register_server_func = ::GetProcAddress(dll_module,
          user_level_registration_ ? kUserRegistrationEntryPoint :
                                     kDefaultRegistrationEntryPoint);
    } else {
      register_server_func = ::GetProcAddress(dll_module,
          user_level_registration_ ? kUserUnregistrationEntryPoint :
                                     kDefaultUnregistrationEntryPoint);
    }

    if (NULL != register_server_func) {
      success = SUCCEEDED(register_server_func());
      if (!success) {
        LOG(ERROR) << "Failed to " << (do_register ? "register" : "unregister")
                   << " DLL at " << dll_path_.c_str();
      }
    }
    ::FreeLibrary(dll_module);
  }
  return success;
}

bool SelfRegWorkItem::Do() {
  return RegisterDll(do_register_);
}

void SelfRegWorkItem::Rollback() {
  RegisterDll(!do_register_);
}
