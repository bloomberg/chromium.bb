// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/close_handle_hook_win.h"

#include <Windows.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_version_info.h"

namespace {

typedef BOOL (WINAPI* CloseHandleType) (HANDLE handle);
CloseHandleType g_close_function = NULL;

// The entry point for CloseHandle interception. This function notifies the
// verifier about the handle that is being closed, and calls the original
// function.
BOOL WINAPI CloseHandleHook(HANDLE handle) {
  base::win::OnHandleBeingClosed(handle);
  return g_close_function(handle);
}

// Keeps track of all the hooks needed to intercept CloseHandle.
class CloseHandleHooks {
 public:
  CloseHandleHooks() {}
  ~CloseHandleHooks() {}

  void AddIATPatch(const base::string16& module);
  void Unpatch();

 private:
  std::vector<base::win::IATPatchFunction*> hooks_;
  DISALLOW_COPY_AND_ASSIGN(CloseHandleHooks);
};
base::LazyInstance<CloseHandleHooks> g_hooks = LAZY_INSTANCE_INITIALIZER;

void CloseHandleHooks::AddIATPatch(const base::string16& module) {
  if (module.empty())
    return;

  base::win::IATPatchFunction* patch = new base::win::IATPatchFunction;
  patch->Patch(module.c_str(), "kernel32.dll", "CloseHandle", CloseHandleHook);
  hooks_.push_back(patch);
  if (!g_close_function) {
    // Things are probably messed up if each intercepted function points to
    // a different place, but we need only one function to call.
    g_close_function =
      reinterpret_cast<CloseHandleType>(patch->original_function());
  }
}

void CloseHandleHooks::Unpatch() {
  for (std::vector<base::win::IATPatchFunction*>::iterator it = hooks_.begin();
       it != hooks_.end(); ++it) {
    (*it)->Unpatch();
  }
}

bool UseHooks() {
  return false;
}

base::string16 GetModuleName(HMODULE module) {
  base::string16 name;
  if (!module)
    return name;
  wchar_t buffer[MAX_PATH];
  int rv = GetModuleFileName(module, buffer, MAX_PATH);
  if (rv == MAX_PATH)
    return name;

  buffer[MAX_PATH - 1] = L'\0';
  name.assign(buffer);
  base::FilePath path(name);
  return path.BaseName().AsUTF16Unsafe();
}

HMODULE GetChromeDLLModule() {
  HMODULE module;
  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<wchar_t*>(&GetChromeDLLModule),
                         &module)) {
    return NULL;
  }
  return module;
}

}  // namespace

void InstallCloseHandleHooks() {
  if (UseHooks()) {
    CloseHandleHooks* hooks = g_hooks.Pointer();
    hooks->AddIATPatch(L"chrome.exe");
    hooks->AddIATPatch(GetModuleName(GetChromeDLLModule()));
  } else {
    base::win::DisableHandleVerifier();
  }
}

void RemoveCloseHandleHooks() {
  g_hooks.Get().Unpatch();
}
