// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#pragma once

#include <string>
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/common/chrome_switches.h"
namespace chromeos {

class BurnLibrary;
class CryptohomeLibrary;
class KeyboardLibrary;
class InputMethodLibrary;
class LibraryLoader;
class LoginLibrary;
class MountLibrary;
class NetworkLibrary;
class PowerLibrary;
class ScreenLockLibrary;
class SpeechSynthesisLibrary;
class SyslogsLibrary;
class SystemLibrary;
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
    // Setter for BurnLibrary.
    void SetBurnLibrary(BurnLibrary* library, bool own);
    // Setter for CryptohomeLibrary.
    void SetCryptohomeLibrary(CryptohomeLibrary* library, bool own);
    // Setter for KeyboardLibrary
    void SetKeyboardLibrary(KeyboardLibrary* library, bool own);
    // Setter for InputMethodLibrary
    void SetInputMethodLibrary(InputMethodLibrary* library, bool own);
    // Setter for LoginLibrary.
    void SetLoginLibrary(LoginLibrary* library, bool own);
    // Setter for MountLibrary.
    void SetMountLibrary(MountLibrary* library, bool own);
    // Setter for NetworkLibrary.
    void SetNetworkLibrary(NetworkLibrary* library, bool own);
    // Setter for PowerLibrary.
    void SetPowerLibrary(PowerLibrary* library, bool own);
    // Setter for ScreenLockLibrary.
    void SetScreenLockLibrary(ScreenLockLibrary* library, bool own);
    // Setter for SpeechSynthesisLibrary.
    void SetSpeechSynthesisLibrary(SpeechSynthesisLibrary* library, bool own);
    // Setter for SyslogsLibrary.
    void SetSyslogsLibrary(SyslogsLibrary* library, bool own);
    // Setter for SystemLibrary.
    void SetSystemLibrary(SystemLibrary* library, bool own);
    // Setter for TouchpadLibrary.
    void SetTouchpadLibrary(TouchpadLibrary* library, bool own);
    // Setter for UpdateLibrary.
    void SetUpdateLibrary(UpdateLibrary* library, bool own);

   private:
    friend class CrosLibrary;
    explicit TestApi(CrosLibrary* library) : library_(library) {}
    CrosLibrary* library_;
  };

  // This gets the CrosLibrary.
  static CrosLibrary* Get();

  // Getter for BurnLibrary.
  BurnLibrary* GetBurnLibrary();

  // Getter for CryptohomeLibrary.
  CryptohomeLibrary* GetCryptohomeLibrary();

  // Getter for KeyboardLibrary
  KeyboardLibrary* GetKeyboardLibrary();

  // Getter for InputMethodLibrary
  InputMethodLibrary* GetInputMethodLibrary();

  // Getter for LoginLibrary.
  LoginLibrary* GetLoginLibrary();

  // Getter for MountLibrary
  MountLibrary* GetMountLibrary();

  // Getter for NetworkLibrary
  NetworkLibrary* GetNetworkLibrary();

  // Getter for PowerLibrary
  PowerLibrary* GetPowerLibrary();

  // Getter for ScreenLockLibrary
  ScreenLockLibrary* GetScreenLockLibrary();

  // This gets the singleton SpeechSynthesisLibrary.
  SpeechSynthesisLibrary* GetSpeechSynthesisLibrary();

  // This gets the singleton SyslogsLibrary.
  SyslogsLibrary* GetSyslogsLibrary();

  // This gets the singleton SystemLibrary.
  SystemLibrary* GetSystemLibrary();

  // This gets the singleton TouchpadLibrary.
  TouchpadLibrary* GetTouchpadLibrary();

  // This gets the singleton UpdateLibrary.
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
  friend struct DefaultSingletonTraits<chromeos::CrosLibrary>;
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

  Library<BurnLibrary> burn_lib_;
  Library<CryptohomeLibrary> crypto_lib_;
  Library<KeyboardLibrary> keyboard_lib_;
  Library<InputMethodLibrary> input_method_lib_;
  Library<LoginLibrary> login_lib_;
  Library<MountLibrary> mount_lib_;
  Library<NetworkLibrary> network_lib_;
  Library<PowerLibrary> power_lib_;
  Library<ScreenLockLibrary> screen_lock_lib_;
  Library<SpeechSynthesisLibrary> speech_synthesis_lib_;
  Library<SyslogsLibrary> syslogs_lib_;
  Library<SystemLibrary> system_lib_;
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

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
