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

class NetworkLibrary;

// This class handles access to sub-parts of ChromeOS library. it provides
// a level of indirection so individual libraries that it exposes can
// be mocked for testing.
class CrosLibrary {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(bool use_stub);

  // Destroys the global instance. Must be called before AtExitManager is
  // destroyed to ensure a clean shutdown.
  static void Shutdown();

  // Gets the global instance. Returns NULL if Initialize() has not been
  // called (or Shutdown() has been called).
  static CrosLibrary* Get();

  // Gets the network library. The network library is created if it's not yet
  // created.
  NetworkLibrary* GetNetworkLibrary();

  // Sets the network library to be returned from GetNetworkLibrary().
  // CrosLibrary will take the ownership. The existing network library will
  // be deleted. Passing NULL will just delete the existing network library.
  void SetNetworkLibrary(NetworkLibrary* network_library);

  // Note: Since we are no longer loading Libcros, we can return true here
  // whenever the used libraries are not stub.
  // TODO(hashimoto): Remove this method.
  bool libcros_loaded() { return !use_stub_impl_; }

 private:
  explicit CrosLibrary(bool use_stub);
  virtual ~CrosLibrary();

  scoped_ptr<NetworkLibrary> network_library_;

  // Stub implementations of the libraries should be used.
  bool use_stub_impl_;

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
