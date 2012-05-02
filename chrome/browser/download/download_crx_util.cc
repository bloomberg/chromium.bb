// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;
using content::DownloadItem;

namespace download_crx_util {

namespace {

// Hold a mock ExtensionInstallUI object that will be used when the
// download system opens a CRX.
ExtensionInstallUI* mock_install_ui_for_testing = NULL;

// Called to get an extension install UI object.  In tests, will return
// a mock if the test calls download_util::SetMockInstallUIForTesting()
// to set one.
ExtensionInstallUI* CreateExtensionInstallUI(Profile* profile) {
  // Use a mock if one is present.  Otherwise, create a real extensions
  // install UI.
  ExtensionInstallUI* result = NULL;
  if (mock_install_ui_for_testing) {
    result = mock_install_ui_for_testing;
    mock_install_ui_for_testing = NULL;
  } else {
    result = new ExtensionInstallUI(profile);
  }

  return result;
}

}  // namespace

// Tests can call this method to inject a mock ExtensionInstallUI
// to be used to confirm permissions on a downloaded CRX.
void SetMockInstallUIForTesting(ExtensionInstallUI* mock_ui) {
  mock_install_ui_for_testing = mock_ui;
}

bool ShouldOpenExtensionDownload(const DownloadItem& download_item) {
  if (extensions::switch_utils::IsOffStoreInstallEnabled() ||
      WebstoreInstaller::GetAssociatedApproval(download_item)) {
    return true;
  } else {
    return false;
  }
}

scoped_refptr<CrxInstaller> OpenChromeExtension(
    Profile* profile,
    const DownloadItem& download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* service = profile->GetExtensionService();
  CHECK(service);

  scoped_refptr<CrxInstaller> installer(
      CrxInstaller::Create(
          service,
          CreateExtensionInstallUI(profile),
          WebstoreInstaller::GetAssociatedApproval(download_item)));
  installer->set_delete_source(true);

  if (UserScript::IsURLUserScript(download_item.GetURL(),
                                  download_item.GetMimeType())) {
    installer->InstallUserScript(download_item.GetFullPath(),
                                 download_item.GetURL());
  } else {
    bool is_gallery_download = service->IsDownloadFromGallery(
        download_item.GetURL(), download_item.GetReferrerUrl());
    installer->set_original_mime_type(download_item.GetOriginalMimeType());
    installer->set_apps_require_extension_mime_type(true);
    installer->set_download_url(download_item.GetURL());
    installer->set_is_gallery_install(is_gallery_download);
    if (is_gallery_download)
      installer->set_original_download_url(download_item.GetOriginalUrl());
    installer->set_allow_silent_install(is_gallery_download);
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);
    installer->InstallCrx(download_item.GetFullPath());
  }

  return installer;
}

}  // namespace download_crx_util
