// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_

#include <string>
#include "base/basictypes.h"
#include "base/singleton.h"

namespace chromeos {

class CryptohomeLibrary;
class LanguageLibrary;
class LibraryLoader;
class LoginLibrary;
class MountLibrary;
class NetworkLibrary;
class PowerLibrary;
class SynapticsLibrary;

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
    // Setter for LibraryLoader.
    void SetLibraryLoader(LibraryLoader* loader);
    // Setter for CryptohomeLibrary.
    void SetCryptohomeLibrary(CryptohomeLibrary* library);
    // Setter for LanguageLibrary
    void SetLanguageLibrary(LanguageLibrary* library);
    // Setter for LoginLibrary.
    void SetLoginLibrary(LoginLibrary* library);
    // Setter for MountLibrary.
    void SetMountLibrary(MountLibrary* library);
    // Setter for NetworkLibrary.
    void SetNetworkLibrary(NetworkLibrary* library);
    // Setter for PowerLibrary.
    void SetPowerLibrary(PowerLibrary* library);
    // Setter for SynapticsLibrary.
    void SetSynapticsLibrary(SynapticsLibrary* library);

   private:
    friend class CrosLibrary;
    explicit TestApi(CrosLibrary* library) : library_(library) {}
    CrosLibrary* library_;
  };

  // This gets the CrosLibrary.
  static CrosLibrary* Get();

  // Getter for CryptohomeLibrary.
  CryptohomeLibrary* GetCryptohomeLibrary();

  // // Getter for LanguageLibrary
  LanguageLibrary* GetLanguageLibrary();

  // Getter for LoginLibrary.
  LoginLibrary* GetLoginLibrary();

  // Getter for MountLibrary
  MountLibrary* GetMountLibrary();

  // Getter for NetworkLibrary
  NetworkLibrary* GetNetworkLibrary();

  // Getter for PowerLibrary
  PowerLibrary* GetPowerLibrary();

  // This gets the singleton SynapticsLibrary.
  SynapticsLibrary* GetSynapticsLibrary();

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
  LanguageLibrary* language_lib_;
  LoginLibrary* login_lib_;
  MountLibrary* mount_lib_;
  NetworkLibrary* network_lib_;
  PowerLibrary* power_lib_;
  SynapticsLibrary* synaptics_lib_;
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
