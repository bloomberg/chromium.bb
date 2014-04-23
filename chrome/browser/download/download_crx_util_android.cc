// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/download_item.h"

using content::BrowserThread;
using content::DownloadItem;

namespace download_crx_util {

scoped_refptr<extensions::CrxInstaller> CreateCrxInstaller(
    Profile* profile,
    const content::DownloadItem& download_item) {
  NOTIMPLEMENTED() << "CrxInstaller not implemented on Android";
  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::CreateSilent(NULL));
  return installer;
}

void SetMockInstallPromptForTesting(ExtensionInstallPrompt* mock_prompt) {
  NOTIMPLEMENTED();
}

scoped_refptr<extensions::CrxInstaller> OpenChromeExtension(
    Profile* profile,
    const DownloadItem& download_item) {
  NOTIMPLEMENTED() << "CrxInstaller not implemented on Android";
  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::CreateSilent(NULL));
  return installer;
}

bool IsExtensionDownload(const DownloadItem& download_item) {
  // Extensions are not supported on Android. We want to treat them as
  // normal file downloads.
  return false;
}

bool OffStoreInstallAllowedByPrefs(Profile* profile,
                                   const content::DownloadItem& item) {
  // Extensions are not supported on Android, return the safe default.
  return false;
}

}  // namespace download_crx_util
