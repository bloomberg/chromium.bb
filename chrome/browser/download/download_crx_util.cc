// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;
using content::DownloadItem;
using extensions::WebstoreInstaller;

namespace download_crx_util {

namespace {

// Hold a mock ExtensionInstallPrompt object that will be used when the
// download system opens a CRX.
ExtensionInstallPrompt* mock_install_prompt_for_testing = NULL;

// Called to get an extension install UI object.  In tests, will return
// a mock if the test calls download_util::SetMockInstallUIForTesting()
// to set one.
ExtensionInstallPrompt* CreateExtensionInstallPrompt(Profile* profile) {
  // Use a mock if one is present.  Otherwise, create a real extensions
  // install UI.
  ExtensionInstallPrompt* result = NULL;
  if (mock_install_prompt_for_testing) {
    result = mock_install_prompt_for_testing;
    mock_install_prompt_for_testing = NULL;
  } else {
    Browser* browser = browser::FindLastActiveWithProfile(profile);
    result = chrome::CreateExtensionInstallPromptWithBrowser(browser);
  }

  return result;
}

bool OffStoreInstallAllowedByPrefs(Profile* profile, const DownloadItem& item) {
  extensions::ExtensionPrefs* prefs = extensions::ExtensionSystem::Get(
      profile)->extension_service()->extension_prefs();
  CHECK(prefs);

  URLPatternSet url_patterns = prefs->GetAllowedInstallSites();

  // TODO(aa): RefererURL is cleared in some cases, for example when going
  // between secure and non-secure URLs. It would be better if DownloadItem
  // tracked the initiating page explicitly.
  return url_patterns.MatchesURL(item.GetURL()) &&
      url_patterns.MatchesURL(item.GetReferrerUrl());
}

}  // namespace

// Tests can call this method to inject a mock ExtensionInstallPrompt
// to be used to confirm permissions on a downloaded CRX.
void SetMockInstallPromptForTesting(ExtensionInstallPrompt* mock_prompt) {
  mock_install_prompt_for_testing = mock_prompt;
}

scoped_refptr<extensions::CrxInstaller> OpenChromeExtension(
    Profile* profile,
    const DownloadItem& download_item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* service = profile->GetExtensionService();
  CHECK(service);

  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::Create(
          service,
          CreateExtensionInstallPrompt(profile),
          WebstoreInstaller::GetAssociatedApproval(download_item)));

  installer->set_error_on_unsupported_requirements(true);
  installer->set_delete_source(true);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);

  if (OffStoreInstallAllowedByPrefs(profile, download_item)) {
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowedBecausePref);
  }

  if (extensions::UserScript::IsURLUserScript(download_item.GetURL(),
                                              download_item.GetMimeType())) {
    installer->InstallUserScript(download_item.GetFullPath(),
                                 download_item.GetURL());
  } else {
    bool is_gallery_download =
        WebstoreInstaller::GetAssociatedApproval(download_item) != NULL;
    installer->set_original_mime_type(download_item.GetOriginalMimeType());
    installer->set_apps_require_extension_mime_type(true);
    installer->set_download_url(download_item.GetURL());
    installer->set_is_gallery_install(is_gallery_download);
    if (is_gallery_download)
      installer->set_original_download_url(download_item.GetOriginalUrl());
    installer->set_allow_silent_install(is_gallery_download);
    installer->InstallCrx(download_item.GetFullPath());
  }

  return installer;
}

bool IsExtensionDownload(const DownloadItem& download_item) {
  if (download_item.GetTargetDisposition() ==
      DownloadItem::TARGET_DISPOSITION_PROMPT)
    return false;

  if (download_item.GetMimeType() == extensions::Extension::kMimeType ||
      extensions::UserScript::IsURLUserScript(download_item.GetURL(),
                                              download_item.GetMimeType())) {
    return true;
  } else {
    return false;
  }
}

}  // namespace download_crx_util
