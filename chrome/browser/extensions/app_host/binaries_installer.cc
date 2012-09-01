// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_host/binaries_installer.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/extensions/app_host/binaries_installer_internal.h"
#include "google_update/google_update_idl.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;

namespace app_host {

namespace {
const int kInstallationPollingIntervalMs = 50;
}  // namespace

// Attempts to install the Chrome Binaries using the IGoogleUpdate3 interface.
// Blocks until the installation process completes, without message pumping.
HRESULT InstallBinaries() {
  base::win::ScopedCOMInitializer initialize_com;

  HRESULT hr = S_OK;
  if (!initialize_com.succeeded()) {
    LOG(ERROR) << "COM initialization failed";
    hr = E_FAIL;
  }

  ScopedComPtr<IGoogleUpdate3> update3;
  if (SUCCEEDED(hr)) {
    hr = internal::CreateGoogleUpdate3(update3.Receive());
  }

  ScopedBstr ap_value;
  if (SUCCEEDED(hr)) {
    hr = internal::SelectBinariesApValue(update3, ap_value.Receive());
  }

  ScopedComPtr<IAppBundle> app_bundle;
  if (SUCCEEDED(hr)) {
    hr = internal::CreateAppBundle(update3, app_bundle.Receive());
  }

  ScopedComPtr<IApp> app;
  if (SUCCEEDED(hr)) {
    hr = internal::CreateBinariesIApp(app_bundle, ap_value, app.Receive());
  }

  if (SUCCEEDED(hr)) {
    hr = app_bundle->checkForUpdate();
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to initiate update check: " << hr;
    }
  }

  if (SUCCEEDED(hr)) {
    // We rely upon Omaha to eventually time out and transition to a failure
    // state.
    while (!internal::CheckIfDone(app_bundle, app, &hr)) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
          kInstallationPollingIntervalMs));
    }
  }

  return hr;
}

}  // namespace app_host
