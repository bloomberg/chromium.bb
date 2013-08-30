// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/open_with_browser.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/webplugininfo.h"
#include "net/base/net_util.h"

using content::BrowserThread;
using content::PluginService;

namespace file_manager {
namespace util {
namespace {

const base::FilePath::CharType kCRXExtension[] = FILE_PATH_LITERAL(".crx");
const base::FilePath::CharType kPdfExtension[] = FILE_PATH_LITERAL(".pdf");
const base::FilePath::CharType kSwfExtension[] = FILE_PATH_LITERAL(".swf");

// List of file extensions viewable in the browser.
const base::FilePath::CharType* kFileExtensionsViewableInBrowser[] = {
#if defined(GOOGLE_CHROME_BUILD)
  FILE_PATH_LITERAL(".pdf"),
  FILE_PATH_LITERAL(".swf"),
#endif
  FILE_PATH_LITERAL(".bmp"),
  FILE_PATH_LITERAL(".jpg"),
  FILE_PATH_LITERAL(".jpeg"),
  FILE_PATH_LITERAL(".png"),
  FILE_PATH_LITERAL(".webp"),
  FILE_PATH_LITERAL(".gif"),
  FILE_PATH_LITERAL(".txt"),
  FILE_PATH_LITERAL(".html"),
  FILE_PATH_LITERAL(".htm"),
  FILE_PATH_LITERAL(".mhtml"),
  FILE_PATH_LITERAL(".mht"),
  FILE_PATH_LITERAL(".svg"),
};

// Returns true if |file_path| is viewable in the browser (ex. HTML file).
bool IsViewableInBrowser(const base::FilePath& file_path) {
  for (size_t i = 0; i < arraysize(kFileExtensionsViewableInBrowser); i++) {
    if (file_path.MatchesExtension(kFileExtensionsViewableInBrowser[i]))
      return true;
  }
  return false;
}

bool IsPepperPluginEnabled(Profile* profile,
                           const base::FilePath& plugin_path) {
  DCHECK(profile);

  content::PepperPluginInfo* pepper_info =
      PluginService::GetInstance()->GetRegisteredPpapiPluginInfo(plugin_path);
  if (!pepper_info)
    return false;

  scoped_refptr<PluginPrefs> plugin_prefs = PluginPrefs::GetForProfile(profile);
  if (!plugin_prefs.get())
    return false;

  return plugin_prefs->IsPluginEnabled(pepper_info->ToWebPluginInfo());
}

bool IsPdfPluginEnabled(Profile* profile) {
  DCHECK(profile);

  base::FilePath plugin_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &plugin_path);
  return IsPepperPluginEnabled(profile, plugin_path);
}

bool IsFlashPluginEnabled(Profile* profile) {
  DCHECK(profile);

  base::FilePath plugin_path(
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath));
  if (plugin_path.empty())
    PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &plugin_path);
  return IsPepperPluginEnabled(profile, plugin_path);
}

void OpenNewTab(Profile* profile, const GURL& url) {
  DCHECK(profile);

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      profile, chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::AddSelectedTabWithURL(browser, url, content::PAGE_TRANSITION_LINK);
  // If the current browser is not tabbed then the new tab will be created
  // in a different browser. Make sure it is visible.
  browser->window()->Show();
}

void InstallCRX(Profile* profile, const base::FilePath& file_path) {
  DCHECK(profile);

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  CHECK(service);

  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::Create(
          service,
          scoped_ptr<ExtensionInstallPrompt>(new ExtensionInstallPrompt(
              profile, NULL, NULL))));
  installer->set_error_on_unsupported_requirements(true);
  installer->set_is_gallery_install(false);
  installer->set_allow_silent_install(false);
  installer->InstallCrx(file_path);
}

// Called when a crx file on Drive was downloaded.
void OnCRXDownloadCallback(Profile* profile,
                           drive::FileError error,
                           const base::FilePath& file,
                           scoped_ptr<drive::ResourceEntry> entry) {
  DCHECK(profile);

  if (error != drive::FILE_ERROR_OK)
    return;
  InstallCRX(profile, file);
}

// Reads the alternate URL from a GDoc file. When it fails, returns a file URL
// for |file_path| as fallback.
// Note that an alternate url is a URL to open a hosted document.
GURL ReadUrlFromGDocOnBlockingPool(const base::FilePath& file_path) {
  GURL url = drive::util::ReadUrlFromGDocFile(file_path);
  if (url.is_empty())
    url = net::FilePathToFileURL(file_path);
  return url;
}

}  // namespace

bool OpenFileWithBrowser(Profile* profile, const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);

  // For things supported natively by the browser, we should open it
  // in a tab.
  if (IsViewableInBrowser(file_path) ||
      ShouldBeOpenedWithPlugin(profile, file_path.Extension())) {
    GURL page_url = net::FilePathToFileURL(file_path);
    // Override drive resource to point to internal handler instead of file URL.
    if (drive::util::IsUnderDriveMountPoint(file_path)) {
      page_url = drive::util::FilePathToDriveURL(
          drive::util::ExtractDrivePath(file_path));
    }
    OpenNewTab(profile, page_url);
    return true;
  }

  if (drive::util::HasGDocFileExtension(file_path)) {
    if (drive::util::IsUnderDriveMountPoint(file_path)) {
      // The file is on Google Docs. Open with drive URL.
      GURL url = drive::util::FilePathToDriveURL(
          drive::util::ExtractDrivePath(file_path));
      OpenNewTab(profile, url);
    } else {
      // The file is local (downloaded from an attachment or otherwise copied).
      // Parse the file to extract the Docs url and open this url.
      base::PostTaskAndReplyWithResult(
          BrowserThread::GetBlockingPool(),
          FROM_HERE,
          base::Bind(&ReadUrlFromGDocOnBlockingPool, file_path),
          base::Bind(&OpenNewTab, static_cast<Profile*>(NULL)));
    }
    return true;
  }

  if (file_path.MatchesExtension(kCRXExtension)) {
    if (drive::util::IsUnderDriveMountPoint(file_path)) {
      drive::DriveIntegrationService* integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile);
      if (!integration_service)
        return false;
      integration_service->file_system()->GetFileByPath(
          drive::util::ExtractDrivePath(file_path),
          base::Bind(&OnCRXDownloadCallback, profile));
    } else {
      InstallCRX(profile, file_path);
    }
    return true;
  }

  // Failed to open the file of unknown type.
  LOG(WARNING) << "Unknown file type: " << file_path.value();
  return false;
}

// If a bundled plugin is enabled, we should open pdf/swf files in a tab.
bool ShouldBeOpenedWithPlugin(
    Profile* profile,
    const base::FilePath::StringType& file_extension) {
  DCHECK(profile);

  const base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe("dummy").AddExtension(file_extension);
  if (file_path.MatchesExtension(kPdfExtension))
    return IsPdfPluginEnabled(profile);
  if (file_path.MatchesExtension(kSwfExtension))
    return IsFlashPluginEnabled(profile);
  return false;
}

}  // namespace util
}  // namespace file_manager
