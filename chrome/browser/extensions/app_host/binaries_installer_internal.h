// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_HOST_BINARIES_INSTALLER_INTERNAL_H_
#define CHROME_BROWSER_EXTENSIONS_APP_HOST_BINARIES_INSTALLER_INTERNAL_H_

#include <windows.h>
#include <oleauto.h>

struct IApp;
struct IAppBundle;
struct IGoogleUpdate3;

namespace app_host {
namespace internal {

// Returns true if the installation process is terminated, in which case *hr
// will reveal the outcome.
bool CheckIfDone(IAppBundle* app_bundle, IApp* app, HRESULT* hr);

// Creates and initializes an IAppBundle using the provided IGoogleUpdate3
// instance.
HRESULT CreateAppBundle(IGoogleUpdate3* update3, IAppBundle** app_bundle);

// Retrieves the AP value of the Application Host, if installed.
HRESULT GetAppHostApValue(IGoogleUpdate3* update3, BSTR* ap_value);

// Adds the Chrome Binaries to the bundle for installation.
HRESULT CreateBinariesIApp(IAppBundle* app_bundle, BSTR ap, IApp** app);

// Create an instance of the IGoogleUpdate3 interface.
HRESULT CreateGoogleUpdate3(IGoogleUpdate3** update3);

// Attempts to use the AP value from the installed App Host, defaulting to Dev
// channel otherwise.
HRESULT SelectBinariesApValue(IGoogleUpdate3* update3, BSTR* ap_value);

}  // namespace internal
}  // namespace app_host

#endif  // CHROME_BROWSER_EXTENSIONS_APP_HOST_BINARIES_INSTALLER_INTERNAL_H_
