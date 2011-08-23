// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/download/download_item.h"
#include "content/common/notification_service.h"

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

scoped_refptr<CrxInstaller> OpenChromeExtension(
    Profile* profile,
    const DownloadItem& download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* service = profile->GetExtensionService();
  CHECK(service);
  NotificationService* nservice = NotificationService::current();
  GURL nonconst_download_url = download_item.GetURL();
  nservice->Notify(chrome::NOTIFICATION_EXTENSION_READY_FOR_INSTALL,
                   Source<DownloadManager>(profile->GetDownloadManager()),
                   Details<GURL>(&nonconst_download_url));

  scoped_refptr<CrxInstaller> installer(
      service->MakeCrxInstaller(CreateExtensionInstallUI(profile)));
  installer->set_delete_source(true);

  if (UserScript::IsURLUserScript(download_item.GetURL(),
                                  download_item.mime_type())) {
    installer->InstallUserScript(download_item.full_path(),
                                 download_item.GetURL());
  } else {
    bool is_gallery_download = service->IsDownloadFromGallery(
        download_item.GetURL(), download_item.referrer_url());
    installer->set_original_mime_type(download_item.original_mime_type());
    installer->set_apps_require_extension_mime_type(true);
    installer->set_original_url(download_item.GetURL());
    installer->set_is_gallery_install(is_gallery_download);
    installer->set_allow_silent_install(is_gallery_download);
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);
    installer->InstallCrx(download_item.full_path());
  }

  return installer;
}

}  // namespace download_crx_util
