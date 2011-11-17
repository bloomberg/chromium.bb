// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"

#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/library_loader.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#include "third_party/cros/chromeos_cros_api.h"

// Pass !libcros_loaded_ to GetDefaultImpl instead of use_stub_impl_ so that
// we load the stub impl regardless of whether use_stub was specified or the
// library failed to load.
#define DEFINE_GET_LIBRARY_METHOD(class_prefix, var_prefix)                    \
class_prefix##Library* CrosLibrary::Get##class_prefix##Library() {             \
  return var_prefix##_lib_.GetDefaultImpl(!libcros_loaded_);                   \
}

#define DEFINE_SET_LIBRARY_METHOD(class_prefix, var_prefix)                    \
void CrosLibrary::TestApi::Set##class_prefix##Library(                         \
    class_prefix##Library* library, bool own) {                                \
  library_->var_prefix##_lib_.SetImpl(library, own);                           \
}

namespace chromeos {

static CrosLibrary* g_cros_library = NULL;

CrosLibrary::CrosLibrary(bool use_stub)
    : library_loader_(NULL),
      own_library_loader_(false),
      use_stub_impl_(use_stub),
      libcros_loaded_(false),
      load_error_(false),
      test_api_(NULL) {
}

CrosLibrary::~CrosLibrary() {
  if (own_library_loader_)
    delete library_loader_;
}

// static
void CrosLibrary::Initialize(bool use_stub) {
  CHECK(!g_cros_library) << "CrosLibrary: Multiple calls to Initialize().";
  g_cros_library = new CrosLibrary(use_stub);
  if (use_stub) {
    VLOG(1) << "CrosLibrary Initialized with Stub Impl.";
    return;
  }
  // Attempt to load libcros here, so that we can log, show warnings, and
  // set load_error_string_ immediately.
  if (g_cros_library->LoadLibcros()) {
    VLOG(1) << "CrosLibrary Initialized, version = " << kCrosAPIVersion;
  } else {
    LOG(WARNING) << "CrosLibrary failed to Initialize."
                 << " Will use stub implementations.";
  }
}

// static
void CrosLibrary::Shutdown() {
  CHECK(g_cros_library) << "CrosLibrary::Shutdown() called with NULL library";
  VLOG(1) << "CrosLibrary Shutting down...";
  delete g_cros_library;
  g_cros_library = NULL;
  VLOG(1) << "  CrosLibrary Shutdown completed.";
}

// static
CrosLibrary* CrosLibrary::Get() {
  return g_cros_library;
}

DEFINE_GET_LIBRARY_METHOD(Burn, burn);
DEFINE_GET_LIBRARY_METHOD(Cert, cert);
DEFINE_GET_LIBRARY_METHOD(Cryptohome, crypto);
DEFINE_GET_LIBRARY_METHOD(Network, network);
DEFINE_GET_LIBRARY_METHOD(Power, power);
DEFINE_GET_LIBRARY_METHOD(ScreenLock, screen_lock);
DEFINE_GET_LIBRARY_METHOD(Update, update);

bool CrosLibrary::LoadLibcros() {
  if (!libcros_loaded_ && !load_error_) {
    if (!library_loader_) {
      library_loader_ = LibraryLoader::GetImpl();
      own_library_loader_ = true;
    }
    libcros_loaded_ = library_loader_->Load(&load_error_string_);
    load_error_ = !libcros_loaded_;
  }
  return libcros_loaded_;
}

CrosLibrary::TestApi* CrosLibrary::GetTestApi() {
  if (!test_api_.get())
    test_api_.reset(new TestApi(this));
  return test_api_.get();
}

void CrosLibrary::TestApi::ResetUseStubImpl() {
  library_->use_stub_impl_ = false;
  if (!library_->LoadLibcros())
    LOG(WARNING) << "ResetUseStubImpl: Unable to load libcros.";
}

void CrosLibrary::TestApi::SetLibraryLoader(LibraryLoader* loader, bool own) {
  if (library_->library_loader_ == loader)
    return;
  if (library_->own_library_loader_)
    delete library_->library_loader_;
  library_->own_library_loader_ = own;
  library_->library_loader_ = loader;
  // Reset load flags when loader changes. Otherwise some tests are really not
  // going to be happy.
  library_->libcros_loaded_ = false;
  library_->load_error_ = false;
}

DEFINE_SET_LIBRARY_METHOD(Cert, cert);
DEFINE_SET_LIBRARY_METHOD(Burn, burn);
DEFINE_SET_LIBRARY_METHOD(Cryptohome, crypto);
DEFINE_SET_LIBRARY_METHOD(Network, network);
DEFINE_SET_LIBRARY_METHOD(Power, power);
DEFINE_SET_LIBRARY_METHOD(ScreenLock, screen_lock);
DEFINE_SET_LIBRARY_METHOD(Update, update);

} // namespace chromeos
