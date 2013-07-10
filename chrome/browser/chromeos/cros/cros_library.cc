// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"

#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

static CrosLibrary* g_cros_library = NULL;

CrosLibrary::CrosLibrary(bool use_stub) : use_stub_impl_(use_stub) {}

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

NetworkLibrary* CrosLibrary::GetNetworkLibrary() {
  if (!network_library_)
    network_library_.reset(NetworkLibrary::GetImpl(use_stub_impl_));
  return network_library_.get();
}

void CrosLibrary::SetNetworkLibrary(NetworkLibrary* library) {
  if (network_library_)
    network_library_.reset();
  network_library_.reset(library);
}

} // namespace chromeos
