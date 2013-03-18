// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_LIBRARY_H_

#include <string>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
}

namespace chromeos {

class CertLibrary;
class CryptohomeLibrary;
class NetworkLibrary;

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
    void SetCertLibrary(CertLibrary* library, bool own);
    void SetCryptohomeLibrary(CryptohomeLibrary* library, bool own);
    void SetNetworkLibrary(NetworkLibrary* library, bool own);

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

  CertLibrary* GetCertLibrary();
  CryptohomeLibrary* GetCryptohomeLibrary();
  NetworkLibrary* GetNetworkLibrary();

  // Getter for Test API that gives access to internal members of this class.
  TestApi* GetTestApi();

  // Note: Since we are no longer loading Libcros, we can return true here
  // whenever the used libraries are not stub.
  // TODO(hashimoto): Remove this method.
  bool libcros_loaded() { return !use_stub_impl_; }

 private:
  friend struct base::DefaultLazyInstanceTraits<chromeos::CrosLibrary>;
  friend class CrosLibrary::TestApi;

  explicit CrosLibrary(bool use_stub);
  virtual ~CrosLibrary();

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

  Library<CertLibrary> cert_lib_;
  Library<CryptohomeLibrary> crypto_lib_;
  Library<NetworkLibrary> network_lib_;

  // Stub implementations of the libraries should be used.
  bool use_stub_impl_;
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
