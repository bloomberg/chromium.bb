// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CRX_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CRX_UTIL_H_

#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class CrxInstaller;
class ExtensionInstallUI;
class Profile;

namespace content {
class DownloadItem;
}

namespace download_crx_util {

// Allow tests to install a mock extension install UI object, to fake
// user clicks on the permissions dialog.  Each installed mock object
// is only used once.  If you want to return a mock for two different
// installs, you need to call this function once before the first
// install, and again after the first install and before the second.
void SetMockInstallUIForTesting(ExtensionInstallUI* mock_ui);

// Start installing a downloaded item item as a CRX (extension, theme, app,
// ...).  The installer does work on the file thread, so the installation
// is not complete when this function returns.  Returns the object managing
// the installation.
scoped_refptr<CrxInstaller> OpenChromeExtension(
    Profile* profile,
    const content::DownloadItem& download_item);

}  // namespace download_crx_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CRX_UTIL_H_
