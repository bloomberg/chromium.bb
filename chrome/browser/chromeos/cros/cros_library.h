// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#pragma once

#include <string>
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_switches.h"

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
}

namespace chromeos {

class BrightnessLibrary;
class BurnLibrary;
class CryptohomeLibrary;
class InputMethodLibrary;
class LibCrosServiceLibrary;
class LibraryLoader;
class LoginLibrary;
class MountLibrary;
class NetworkLibrary;
class PowerLibrary;
class ScreenLockLibrary;
class SpeechSynthesisLibrary;
class SyslogsLibrary;
class TouchpadLibrary;
class UpdateLibrary;

// This class handles access to sub-parts of ChromeOS library. it provides
// a level of indirection so individual libraries that it exposes can
// be mocked for testing.
class CrosLibrary {
 public:
  // This class provides access to internal members of CrosLibrary class for
  // purpose of testing (i.e. replacement of members' implementation with
  // mock objects).
  class TestApi {
   public:
    // Use the stub implementations of the library. This is mainly for
    // running the chromeos build of chrome on the desktop.
    void SetUseStubImpl();

    // Reset the stub implementations of the library, called after
    // SetUseStubImp is called.
    void ResetUseStubImpl();

    // Passing true for own for these setters will cause them to be deleted
    // when the CrosLibrary is deleted (or other mocks are set).
    // Setter for LibraryLoader.
    void SetLibraryLoader(LibraryLoader* loader, bool own);
    void SetBrightnessLibrary(BrightnessLibrary* library, bool own);
    void SetBurnLibrary(BurnLibrary* library, bool own);
    void SetCryptohomeLibrary(CryptohomeLibrary* library, bool own);
    void SetInputMethodLibrary(InputMethodLibrary* library, bool own);
    void SetLibCrosServiceLibrary(LibCrosServiceLibrary* library, bool own);
    void SetLoginLibrary(LoginLibrary* library, bool own);
    void SetMountLibrary(MountLibrary* library, bool own);
    void SetNetworkLibrary(NetworkLibrary* library, bool own);
    void SetPowerLibrary(PowerLibrary* library, bool own);
    void SetScreenLockLibrary(ScreenLockLibrary* library, bool own);
    void SetSpeechSynthesisLibrary(SpeechSynthesisLibrary* library, bool own);
    void SetSyslogsLibrary(SyslogsLibrary* library, bool own);
    void SetTouchpadLibrary(TouchpadLibrary* library, bool own);
    void SetUpdateLibrary(UpdateLibrary* library, bool own);

   private:
    friend class CrosLibrary;
    explicit TestApi(CrosLibrary* library) : library_(library) {}
    CrosLibrary* library_;
  };

  // This gets the CrosLibrary.
  static CrosLibrary* Get();

  BrightnessLibrary* GetBrightnessLibrary();
  BurnLibrary* GetBurnLibrary();
  CryptohomeLibrary* GetCryptohomeLibrary();
  InputMethodLibrary* GetInputMethodLibrary();
  LibCrosServiceLibrary* GetLibCrosServiceLibrary();
  LoginLibrary* GetLoginLibrary();
  MountLibrary* GetMountLibrary();
  NetworkLibrary* GetNetworkLibrary();
  PowerLibrary* GetPowerLibrary();
  ScreenLockLibrary* GetScreenLockLibrary();
  SpeechSynthesisLibrary* GetSpeechSynthesisLibrary();
  SyslogsLibrary* GetSyslogsLibrary();
  TouchpadLibrary* GetTouchpadLibrary();
  UpdateLibrary* GetUpdateLibrary();

  // Getter for Test API that gives access to internal members of this class.
  TestApi* GetTestApi();

  // Ensures that the library is loaded, loading it if needed. If the library
  // could not be loaded, returns false.
  bool EnsureLoaded();

  // Returns an unlocalized string describing the last load error (if any).
  const std::string& load_error_string() {
    return load_error_string_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<chromeos::CrosLibrary>;
  friend class CrosLibrary::TestApi;

  CrosLibrary();
  virtual ~CrosLibrary();

  LibraryLoader* library_loader_;

  bool own_library_loader_;

  // This template supports the creation, setting and optional deletion of
  // the cros libraries.
  template <class L>
  class Library {
   public:
    Library() : library_(NULL), own_(true) {}

    ~Library() {
      if (own_)
        delete library_;
    }

    L* GetDefaultImpl(bool use_stub_impl) {
      if (!library_) {
        own_ = true;
        if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kForceStubLibcros))
          use_stub_impl = true;
        library_ = L::GetImpl(use_stub_impl);
      }
      return library_;
    }

    void SetImpl(L* library, bool own) {
      if (library != library_) {
        if (own_)
          delete library_;
        library_ = library;
        own_ = own;
      }
    }

   private:
    L* library_;
    bool own_;
  };

  Library<BrightnessLibrary> brightness_lib_;
  Library<BurnLibrary> burn_lib_;
  Library<CryptohomeLibrary> crypto_lib_;
  Library<InputMethodLibrary> input_method_lib_;
  Library<LibCrosServiceLibrary> libcros_service_lib_;
  Library<LoginLibrary> login_lib_;
  Library<MountLibrary> mount_lib_;
  Library<NetworkLibrary> network_lib_;
  Library<PowerLibrary> power_lib_;
  Library<ScreenLockLibrary> screen_lock_lib_;
  Library<SpeechSynthesisLibrary> speech_synthesis_lib_;
  Library<SyslogsLibrary> syslogs_lib_;
  Library<TouchpadLibrary> touchpad_lib_;
  Library<UpdateLibrary> update_lib_;

  // Stub implementations of the libraries should be used.
  bool use_stub_impl_;
  // True if libcros was successfully loaded.
  bool loaded_;
  // True if the last load attempt had an error.
  bool load_error_;
  // Contains the error string from the last load attempt.
  std::string load_error_string_;
  scoped_ptr<TestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(CrosLibrary);
};

// The class is used for enabling the stub libcros, and cleaning it up at
// the end of the object lifetime. Useful for testing.
class ScopedStubCrosEnabler {
 public:
  ScopedStubCrosEnabler() {
    chromeos::CrosLibrary::Get()->GetTestApi()->SetUseStubImpl();
  }

  ~ScopedStubCrosEnabler() {
    chromeos::CrosLibrary::Get()->GetTestApi()->ResetUseStubImpl();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedStubCrosEnabler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
