// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"

#include "chrome/browser/chromeos/cros/network_library.h"

#define DEFINE_GET_LIBRARY_METHOD(class_prefix, var_prefix)                    \
class_prefix##Library* CrosLibrary::Get##class_prefix##Library() {             \
  return var_prefix##_lib_.GetDefaultImpl(use_stub_impl_);                     \
}

#define DEFINE_SET_LIBRARY_METHOD(class_prefix, var_prefix)                    \
void CrosLibrary::TestApi::Set##class_prefix##Library(                         \
    class_prefix##Library* library, bool own) {                                \
  library_->var_prefix##_lib_.SetImpl(library, own);                           \
}

namespace chromeos {

static CrosLibrary* g_cros_library = NULL;

CrosLibrary::CrosLibrary(bool use_stub)
    : use_stub_impl_(use_stub),
      test_api_(NULL) {
}

CrosLibrary::~CrosLibrary() {
}

// static
void CrosLibrary::Initialize(bool use_stub) {
  CHECK(!g_cros_library) << "CrosLibrary: Multiple calls to Initialize().";
  g_cros_library = new CrosLibrary(use_stub);
  VLOG_IF(1, use_stub) << "CrosLibrary Initialized with Stub Impl.";
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

DEFINE_GET_LIBRARY_METHOD(Network, network);

CrosLibrary::TestApi* CrosLibrary::GetTestApi() {
  if (!test_api_.get())
    test_api_.reset(new TestApi(this));
  return test_api_.get();
}

DEFINE_SET_LIBRARY_METHOD(Network, network);

} // namespace chromeos
