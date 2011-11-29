// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#pragma once

#include <string>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
}

namespace chromeos {

class BurnLibrary;
class CertLibrary;
class CryptohomeLibrary;
class LibraryLoader;
class NetworkLibrary;
class ScreenLockLibrary;

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
    // Resets the stub implementation of the library and loads libcros.
    // Used by tests that need to run with libcros (i.e. on a device).
    void ResetUseStubImpl();

    // Passing true for own for these setters will cause them to be deleted
    // when the CrosLibrary is deleted (or other mocks are set).
    // Setter for LibraryLoader.
    void SetLibraryLoader(LibraryLoader* loader, bool own);
    void SetCertLibrary(CertLibrary* library, bool own);
    void SetBurnLibrary(BurnLibrary* library, bool own);
    void SetCryptohomeLibrary(CryptohomeLibrary* library, bool own);
    void SetNetworkLibrary(NetworkLibrary* library, bool own);
    void SetScreenLockLibrary(ScreenLockLibrary* library, bool own);

   private:
    friend class CrosLibrary;
    explicit TestApi(CrosLibrary* library) : library_(library) {}
    CrosLibrary* library_;
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(bool use_stub);

  // Destroys the global instance. Must be called before AtExitManager is
  // destroyed to ensure a clean shutdown.
  static void Shutdown();

  // Gets the global instance. Returns NULL if Initialize() has not been
  // called (or Shutdown() has been called).
  static CrosLibrary* Get();

  BurnLibrary* GetBurnLibrary();
  CertLibrary* GetCertLibrary();
  CryptohomeLibrary* GetCryptohomeLibrary();
  NetworkLibrary* GetNetworkLibrary();
  ScreenLockLibrary* GetScreenLockLibrary();

  // Getter for Test API that gives access to internal members of this class.
  TestApi* GetTestApi();

  bool libcros_loaded() { return libcros_loaded_; }

  // Returns an unlocalized string describing the last load error (if any).
  const std::string& load_error_string() {
    return load_error_string_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<chromeos::CrosLibrary>;
  friend class CrosLibrary::TestApi;

  explicit CrosLibrary(bool use_stub);
  virtual ~CrosLibrary();

  bool LoadLibcros();

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
  Library<CertLibrary> cert_lib_;
  Library<CryptohomeLibrary> crypto_lib_;
  Library<NetworkLibrary> network_lib_;
  Library<ScreenLockLibrary> screen_lock_lib_;

  // Stub implementations of the libraries should be used.
  bool use_stub_impl_;
  // True if libcros was successfully loaded.
  bool libcros_loaded_;
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
    chromeos::CrosLibrary::Initialize(true);
  }

  ~ScopedStubCrosEnabler() {
    chromeos::CrosLibrary::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedStubCrosEnabler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
