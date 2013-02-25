// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_UTILS_H_
#define CHROME_FRAME_TEST_UTILS_H_

#include <string>

#include <atlbase.h>
#include <atlcom.h>

#include "base/string16.h"

namespace base {
class FilePath;
}

extern const wchar_t kChromeFrameDllName[];
extern const wchar_t kChromeLauncherExeName[];

// Helper class used to register different chrome frame DLLs while running
// tests. The default constructor registers the DLL found in the build path.
// Programs that use this class MUST include a call to the class's
// RegisterAndExitProcessIfDirected method at the top of their main entrypoint.
//
// At destruction, again registers the DLL found in the build path if another
// DLL has since been registered. Triggers GTEST asserts on failure.
//
// TODO(robertshield): Ideally, make this class restore the originally
// registered chrome frame DLL (e.g. by looking in HKCR) on destruction.
class ScopedChromeFrameRegistrar {
 public:
  enum RegistrationType {
    PER_USER,
    SYSTEM_LEVEL,
  };

  explicit ScopedChromeFrameRegistrar(RegistrationType registration_type);
  ScopedChromeFrameRegistrar(const std::wstring& path,
                             RegistrationType registration_type);
  virtual ~ScopedChromeFrameRegistrar();

  void RegisterChromeFrameAtPath(const std::wstring& path);
  void UnegisterChromeFrameAtPath(const std::wstring& path);
  void RegisterReferenceChromeFrameBuild();

  std::wstring GetChromeFrameDllPath() const;

  static void RegisterAtPath(const std::wstring& path,
                             RegistrationType registration_type);
  static void UnregisterAtPath(const std::wstring& path,
                               RegistrationType registration_type);
  static void RegisterDefaults();
  static base::FilePath GetReferenceChromeFrameDllPath();

  // Registers or unregisters a COM DLL and exits the process if the process's
  // command line is:
  // this.exe --call-registration-entrypoint path_to_dll entrypoint
  // Otherwise simply returns. This method should be invoked at the start of the
  // entrypoint in each executable that uses ScopedChromeFrameRegistrar to
  // register or unregister DLLs.
  static void RegisterAndExitProcessIfDirected();

 private:
  enum RegistrationOperation {
    REGISTER,
    UNREGISTER,
  };

  // The string "--call-registration-entrypoint".
  static const wchar_t kCallRegistrationEntrypointSwitch[];

  static void DoRegistration(const string16& path,
                             RegistrationType registration_type,
                             RegistrationOperation registration_operation);

  // Contains the path of the most recently registered Chrome Frame DLL.
  std::wstring new_chrome_frame_dll_path_;

  // Contains the path of the Chrome Frame DLL to be registered at destruction.
  std::wstring original_dll_path_;

  // Indicates whether per user or per machine registration is needed.
  RegistrationType registration_type_;
  // We need to register the chrome path provider only once per process. This
  // flag keeps track of that.
  static bool register_chrome_path_provider_;
};

// Returns the path to the Chrome Frame DLL in the build directory. Assumes
// that the test executable is running from the build folder or a similar
// folder structure.
base::FilePath GetChromeFrameBuildPath();

// Callback description for onload, onloaderror, onmessage
static _ATL_FUNC_INFO g_single_param = {CC_STDCALL, VT_EMPTY, 1, {VT_VARIANT}};
// Simple class that forwards the callbacks.
template <typename T>
class DispCallback
    : public IDispEventSimpleImpl<1, DispCallback<T>, &IID_IDispatch> {
 public:
  typedef HRESULT (T::*Method)(const VARIANT* param);

  DispCallback(T* owner, Method method) : owner_(owner), method_(method) {
  }

  BEGIN_SINK_MAP(DispCallback)
    SINK_ENTRY_INFO(1, IID_IDispatch, DISPID_VALUE, OnCallback, &g_single_param)
  END_SINK_MAP()

  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return owner_->AddRef();
  }
  virtual ULONG STDMETHODCALLTYPE Release() {
    return owner_->Release();
  }

  STDMETHOD(OnCallback)(VARIANT param) {
    return (owner_->*method_)(&param);
  }

  IDispatch* ToDispatch() {
    return reinterpret_cast<IDispatch*>(this);
  }

  T* owner_;
  Method method_;
};

// If the workstation is locked and cannot receive user input.
bool IsWorkstationLocked();

#endif  // CHROME_FRAME_TEST_UTILS_H_
