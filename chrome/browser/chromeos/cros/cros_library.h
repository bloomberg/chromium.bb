// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_

#include <string>
#include "base/basictypes.h"
#include "base/singleton.h"

namespace chromeos {

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
class SynapticsLibrary;
class SyslogsLibrary;
class SystemLibrary;
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
    // Passing true for own for these setters will cause them to be deleted
    // when the CrosLibrary is deleted (or other mocks are set).
    // Setter for LibraryLoader.
    void SetLibraryLoader(LibraryLoader* loader, bool own);
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
    // Setter for SynapticsLibrary.
    void SetSynapticsLibrary(SynapticsLibrary* library, bool own);
    // Setter for SyslogsLibrary.
    void SetSyslogsLibrary(SyslogsLibrary* library, bool own);
    // Setter for SystemLibrary.
    void SetSystemLibrary(SystemLibrary* library, bool own);
    // Setter for UpdateLibrary.
    void SetUpdateLibrary(UpdateLibrary* library, bool own);

   private:
    friend class CrosLibrary;
    explicit TestApi(CrosLibrary* library) : library_(library) {}
    CrosLibrary* library_;
  };

  // This gets the CrosLibrary.
  static CrosLibrary* Get();

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

  // This gets the singleton SynapticsLibrary.
  SynapticsLibrary* GetSynapticsLibrary();

  // This gets the singleton SyslogsLibrary.
  SyslogsLibrary* GetSyslogsLibrary();

  // This gets the singleton SystemLibrary.
  SystemLibrary* GetSystemLibrary();

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
  CryptohomeLibrary* crypto_lib_;
  KeyboardLibrary* keyboard_lib_;
  InputMethodLibrary* input_method_lib_;
  LoginLibrary* login_lib_;
  MountLibrary* mount_lib_;
  NetworkLibrary* network_lib_;
  PowerLibrary* power_lib_;
  ScreenLockLibrary* screen_lock_lib_;
  SpeechSynthesisLibrary* speech_synthesis_lib_;
  SynapticsLibrary* synaptics_lib_;
  SyslogsLibrary* syslogs_lib_;
  SystemLibrary* system_lib_;
  UpdateLibrary* update_lib_;

  bool own_library_loader_;
  bool own_cryptohome_lib_;
  bool own_keyboard_lib_;
  bool own_input_method_lib_;
  bool own_login_lib_;
  bool own_mount_lib_;
  bool own_network_lib_;
  bool own_power_lib_;
  bool own_screen_lock_lib_;
  bool own_speech_synthesis_lib_;
  bool own_synaptics_lib_;
  bool own_syslogs_lib_;
  bool own_system_lib_;
  bool own_update_lib_;

  // True if libcros was successfully loaded.
  bool loaded_;
  // True if the last load attempt had an error.
  bool load_error_;
  // Contains the error string from the last load attempt.
  std::string load_error_string_;
  TestApi* test_api_;

  DISALLOW_COPY_AND_ASSIGN(CrosLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
