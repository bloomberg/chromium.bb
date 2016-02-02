// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_skia.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "ui/gfx/icon_util.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/tab_helper.h"
#include "components/favicon/content/content_favicon_driver.h"
#endif

using content::BrowserThread;

namespace {

#if defined(OS_MACOSX)
const int kDesiredSizes[] = {16, 32, 128, 256, 512};
const size_t kNumDesiredSizes = arraysize(kDesiredSizes);
#elif defined(OS_LINUX)
// Linux supports icons of any size. FreeDesktop Icon Theme Specification states
// that "Minimally you should install a 48x48 icon in the hicolor theme."
const int kDesiredSizes[] = {16, 32, 48, 128, 256, 512};
const size_t kNumDesiredSizes = arraysize(kDesiredSizes);
#elif defined(OS_WIN)
const int* kDesiredSizes = IconUtil::kIconDimensions;
const size_t kNumDesiredSizes = IconUtil::kNumIconDimensions;
#else
const int kDesiredSizes[] = {32};
const size_t kNumDesiredSizes = arraysize(kDesiredSizes);
#endif

#if defined(TOOLKIT_VIEWS)
// Predicator for sorting images from largest to smallest.
bool IconPrecedes(const WebApplicationInfo::IconInfo& left,
                  const WebApplicationInfo::IconInfo& right) {
  return left.width < right.width;
}
#endif

base::FilePath GetShortcutDataDir(const web_app::ShortcutInfo& shortcut_info) {
  return web_app::GetWebAppDataDirectory(shortcut_info.profile_path,
                                         shortcut_info.extension_id,
                                         shortcut_info.url);
}

void UpdateAllShortcutsForShortcutInfo(
    const base::string16& old_app_title,
    const base::Closure& callback,
    scoped_ptr<web_app::ShortcutInfo> shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  base::FilePath shortcut_data_dir = GetShortcutDataDir(*shortcut_info);
  base::Closure task = base::Bind(
      &web_app::internals::UpdatePlatformShortcuts, shortcut_data_dir,
      old_app_title, base::Passed(&shortcut_info), file_handlers_info);
  if (callback.is_null()) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, task);
  } else {
    BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE, task,
                                    callback);
  }
}

void OnImageLoaded(scoped_ptr<web_app::ShortcutInfo> shortcut_info,
                   extensions::FileHandlersInfo file_handlers_info,
                   web_app::InfoCallback callback,
                   const gfx::ImageFamily& image_family) {
  // If the image failed to load (e.g. if the resource being loaded was empty)
  // use the standard application icon.
  if (image_family.empty()) {
    gfx::Image default_icon =
        ResourceBundle::GetSharedInstance().GetImageNamed(IDR_APP_DEFAULT_ICON);
    int size = kDesiredSizes[kNumDesiredSizes - 1];
    SkBitmap bmp = skia::ImageOperations::Resize(
          *default_icon.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST,
          size, size);
    gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bmp);
    // We are on the UI thread, and this image is needed from the FILE thread,
    // for creating shortcut icon files.
    image_skia.MakeThreadSafe();
    shortcut_info->favicon.Add(gfx::Image(image_skia));
  } else {
    shortcut_info->favicon = image_family;
  }

  callback.Run(std::move(shortcut_info), file_handlers_info);
}

void IgnoreFileHandlersInfo(
    const web_app::ShortcutInfoCallback& shortcut_info_callback,
    scoped_ptr<web_app::ShortcutInfo> shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  shortcut_info_callback.Run(std::move(shortcut_info));
}

void ScheduleCreatePlatformShortcut(
    web_app::ShortcutCreationReason reason,
    const web_app::ShortcutLocations& locations,
    scoped_ptr<web_app::ShortcutInfo> shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  base::FilePath shortcut_data_dir = GetShortcutDataDir(*shortcut_info);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&web_app::internals::CreatePlatformShortcuts),
          shortcut_data_dir, base::Passed(&shortcut_info), file_handlers_info,
          locations, reason));
}

}  // namespace

namespace web_app {

// The following string is used to build the directory name for
// shortcuts to chrome applications (the kind which are installed
// from a CRX).  Application shortcuts to URLs use the {host}_{path}
// for the name of this directory.  Hosts can't include an underscore.
// By starting this string with an underscore, we ensure that there
// are no naming conflicts.
static const char kCrxAppPrefix[] = "_crx_";

namespace internals {

base::FilePath GetSanitizedFileName(const base::string16& name) {
#if defined(OS_WIN)
  base::string16 file_name = name;
#else
  std::string file_name = base::UTF16ToUTF8(name);
#endif
  base::i18n::ReplaceIllegalCharactersInPath(&file_name, '_');
  return base::FilePath(file_name);
}

}  // namespace internals

ShortcutInfo::ShortcutInfo() {}
ShortcutInfo::~ShortcutInfo() {}

ShortcutLocations::ShortcutLocations()
    : on_desktop(false),
      applications_menu_location(APP_MENU_LOCATION_NONE),
      in_quick_launch_bar(false) {
}

#if defined(TOOLKIT_VIEWS)
scoped_ptr<ShortcutInfo> GetShortcutInfoForTab(
    content::WebContents* web_contents) {
  const favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  const extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(web_contents);
  const WebApplicationInfo& app_info = extensions_tab_helper->web_app_info();

  scoped_ptr<ShortcutInfo> info(new ShortcutInfo);
  info->url = app_info.app_url.is_empty() ? web_contents->GetURL() :
                                            app_info.app_url;
  info->title = app_info.title.empty() ?
      (web_contents->GetTitle().empty() ? base::UTF8ToUTF16(info->url.spec()) :
                                          web_contents->GetTitle()) :
      app_info.title;
  info->description = app_info.description;
  info->favicon.Add(favicon_driver->GetFavicon());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  info->profile_path = profile->GetPath();

  return info;
}
#endif

#if !defined(OS_WIN)
void UpdateShortcutForTabContents(content::WebContents* web_contents) {}
#endif

scoped_ptr<ShortcutInfo> ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* app,
    Profile* profile) {
  scoped_ptr<ShortcutInfo> shortcut_info(new ShortcutInfo);
  shortcut_info->extension_id = app->id();
  shortcut_info->is_platform_app = app->is_platform_app();

  // Some default-installed apps are converted into bookmark apps on Chrome
  // first run. These should not be considered as being created (by the user)
  // from a web page.
  shortcut_info->from_bookmark =
      app->from_bookmark() && !app->was_installed_by_default();

  shortcut_info->url = extensions::AppLaunchInfo::GetLaunchWebURL(app);
  shortcut_info->title = base::UTF8ToUTF16(app->name());
  shortcut_info->description = base::UTF8ToUTF16(app->description());
  shortcut_info->extension_path = app->path();
  shortcut_info->profile_path = profile->GetPath();
  shortcut_info->profile_name =
      profile->GetPrefs()->GetString(prefs::kProfileName);
  shortcut_info->version_for_display = app->GetVersionForDisplay();
  return shortcut_info;
}

void GetInfoForApp(const extensions::Extension* extension,
                   Profile* profile,
                   const InfoCallback& callback) {
  scoped_ptr<web_app::ShortcutInfo> shortcut_info(
      web_app::ShortcutInfoForExtensionAndProfile(extension, profile));
  const std::vector<extensions::FileHandlerInfo>* file_handlers =
      extensions::FileHandlers::GetFileHandlers(extension);
  extensions::FileHandlersInfo file_handlers_info =
      file_handlers ? *file_handlers : extensions::FileHandlersInfo();

  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < kNumDesiredSizes; ++i) {
    int size = kDesiredSizes[i];
    extensions::ExtensionResource resource =
        extensions::IconsInfo::GetIconResource(
            extension, size, ExtensionIconSet::MATCH_EXACTLY);
    if (!resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
          resource,
          extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
          gfx::Size(size, size),
          ui::SCALE_FACTOR_100P));
    }
  }

  if (info_list.empty()) {
    size_t i = kNumDesiredSizes - 1;
    int size = kDesiredSizes[i];

    // If there is no icon at the desired sizes, we will resize what we can get.
    // Making a large icon smaller is preferred to making a small icon larger,
    // so look for a larger icon first:
    extensions::ExtensionResource resource =
        extensions::IconsInfo::GetIconResource(
            extension, size, ExtensionIconSet::MATCH_BIGGER);
    if (resource.empty()) {
      resource = extensions::IconsInfo::GetIconResource(
          extension, size, ExtensionIconSet::MATCH_SMALLER);
    }
    info_list.push_back(extensions::ImageLoader::ImageRepresentation(
        resource,
        extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
        gfx::Size(size, size),
        ui::SCALE_FACTOR_100P));
  }

  // |info_list| may still be empty at this point, in which case
  // LoadImageFamilyAsync will call the OnImageLoaded callback with an empty
  // image and exit immediately.
  extensions::ImageLoader::Get(profile)->LoadImageFamilyAsync(
      extension, info_list,
      base::Bind(&OnImageLoaded, base::Passed(&shortcut_info),
                 file_handlers_info, callback));
}

void GetShortcutInfoForApp(const extensions::Extension* extension,
                           Profile* profile,
                           const ShortcutInfoCallback& callback) {
  GetInfoForApp(
      extension, profile, base::Bind(&IgnoreFileHandlersInfo, callback));
}

bool ShouldCreateShortcutFor(web_app::ShortcutCreationReason reason,
                             Profile* profile,
                             const extensions::Extension* extension) {
  // Shortcuts should never be created for component apps, or for apps that
  // cannot be shown in the launcher.
  if (extension->location() == extensions::Manifest::COMPONENT ||
      !extensions::ui_util::CanDisplayInAppLauncher(extension, profile)) {
    return false;
  }

  // Always create shortcuts for v2 packaged apps.
  if (extension->is_platform_app())
    return true;

#if defined(OS_MACOSX)
  // A bookmark app installs itself as an extension, then automatically triggers
  // a shortcut request with SHORTCUT_CREATION_AUTOMATED. Allow this flow, but
  // do not automatically create shortcuts for default-installed extensions,
  // until it is explicitly requested by the user.
  if (extension->was_installed_by_default() &&
      reason == SHORTCUT_CREATION_AUTOMATED)
    return false;

  if (extension->from_bookmark())
    return true;

  // Otherwise, don't create shortcuts for automated codepaths.
  if (reason == SHORTCUT_CREATION_AUTOMATED)
    return false;

  if (extension->is_hosted_app()) {
    return !base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableHostedAppShimCreation);
  }

  // Only reached for "legacy" packaged apps. Default to false on Mac.
  return false;
#else
  // For other platforms, allow shortcut creation if it was explicitly
  // requested by the user (i.e. is not automatic).
  return reason == SHORTCUT_CREATION_BY_USER;
#endif
}

base::FilePath GetWebAppDataDirectory(const base::FilePath& profile_path,
                                      const std::string& extension_id,
                                      const GURL& url) {
  DCHECK(!profile_path.empty());
  base::FilePath app_data_dir(profile_path.Append(chrome::kWebAppDirname));

  if (!extension_id.empty()) {
    return app_data_dir.AppendASCII(
        GenerateApplicationNameFromExtensionId(extension_id));
  }

  std::string host(url.host());
  std::string scheme(url.has_scheme() ? url.scheme() : "http");
  std::string port(url.has_port() ? url.port() : "80");
  std::string scheme_port(scheme + "_" + port);

#if defined(OS_WIN)
  base::FilePath::StringType host_path(base::UTF8ToUTF16(host));
  base::FilePath::StringType scheme_port_path(base::UTF8ToUTF16(scheme_port));
#elif defined(OS_POSIX)
  base::FilePath::StringType host_path(host);
  base::FilePath::StringType scheme_port_path(scheme_port);
#endif

  return app_data_dir.Append(host_path).Append(scheme_port_path);
}

base::FilePath GetWebAppDataDirectory(const base::FilePath& profile_path,
                                      const extensions::Extension& extension) {
  return GetWebAppDataDirectory(
      profile_path,
      extension.id(),
      GURL(extensions::AppLaunchInfo::GetLaunchWebURL(&extension)));
}

std::string GenerateApplicationNameFromInfo(const ShortcutInfo& shortcut_info) {
  if (!shortcut_info.extension_id.empty())
    return GenerateApplicationNameFromExtensionId(shortcut_info.extension_id);
  else
    return GenerateApplicationNameFromURL(shortcut_info.url);
}

std::string GenerateApplicationNameFromURL(const GURL& url) {
  std::string t;
  t.append(url.host());
  t.append("_");
  t.append(url.path());
  return t;
}

std::string GenerateApplicationNameFromExtensionId(const std::string& id) {
  std::string t(kCrxAppPrefix);
  t.append(id);
  return t;
}

std::string GetExtensionIdFromApplicationName(const std::string& app_name) {
  std::string prefix(kCrxAppPrefix);
  if (app_name.substr(0, prefix.length()) != prefix)
    return std::string();
  return app_name.substr(prefix.length());
}

void CreateShortcutsWithInfo(
    ShortcutCreationReason reason,
    const ShortcutLocations& locations,
    scoped_ptr<ShortcutInfo> shortcut_info,
    const extensions::FileHandlersInfo& file_handlers_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the shortcut is for an application shortcut with the new bookmark app
  // flow disabled, there will be no corresponding extension.
  if (!shortcut_info->extension_id.empty()) {
    // It's possible for the extension to be deleted before we get here.
    // For example, creating a hosted app from a website. Double check that
    // it still exists.
    Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
        shortcut_info->profile_path);
    if (!profile)
      return;

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    const extensions::Extension* extension = registry->GetExtensionById(
        shortcut_info->extension_id, extensions::ExtensionRegistry::EVERYTHING);
    if (!extension)
      return;
  }

  ScheduleCreatePlatformShortcut(reason, locations, std::move(shortcut_info),
                                 file_handlers_info);
}

void CreateNonAppShortcut(const ShortcutLocations& locations,
                          scoped_ptr<ShortcutInfo> shortcut_info) {
  ScheduleCreatePlatformShortcut(SHORTCUT_CREATION_AUTOMATED, locations,
                                 std::move(shortcut_info),
                                 extensions::FileHandlersInfo());
}

void CreateShortcuts(ShortcutCreationReason reason,
                     const ShortcutLocations& locations,
                     Profile* profile,
                     const extensions::Extension* app) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ShouldCreateShortcutFor(reason, profile, app))
    return;

  GetInfoForApp(
      app, profile, base::Bind(&CreateShortcutsWithInfo, reason, locations));
}

void DeleteAllShortcuts(Profile* profile, const extensions::Extension* app) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_ptr<ShortcutInfo> shortcut_info(
      ShortcutInfoForExtensionAndProfile(app, profile));
  base::FilePath shortcut_data_dir = GetShortcutDataDir(*shortcut_info);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&web_app::internals::DeletePlatformShortcuts,
                 shortcut_data_dir, base::Passed(&shortcut_info)));
}

void UpdateAllShortcuts(const base::string16& old_app_title,
                        Profile* profile,
                        const extensions::Extension* app,
                        const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetInfoForApp(app, profile, base::Bind(&UpdateAllShortcutsForShortcutInfo,
                                         old_app_title, callback));
}

bool IsValidUrl(const GURL& url) {
  static const char* const kValidUrlSchemes[] = {
      url::kFileScheme,
      url::kFileSystemScheme,
      url::kFtpScheme,
      url::kHttpScheme,
      url::kHttpsScheme,
      extensions::kExtensionScheme,
  };

  for (size_t i = 0; i < arraysize(kValidUrlSchemes); ++i) {
    if (url.SchemeIs(kValidUrlSchemes[i]))
      return true;
  }

  return false;
}

#if defined(TOOLKIT_VIEWS)
void GetIconsInfo(const WebApplicationInfo& app_info,
                  IconInfoList* icons) {
  DCHECK(icons);

  icons->clear();
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    // We only take square shaped icons (i.e. width == height).
    if (app_info.icons[i].width == app_info.icons[i].height) {
      icons->push_back(app_info.icons[i]);
    }
  }

  std::sort(icons->begin(), icons->end(), &IconPrecedes);
}
#endif

#if defined(OS_LINUX)
std::string GetWMClassFromAppName(std::string app_name) {
  base::i18n::ReplaceIllegalCharactersInPath(&app_name, '_');
  base::TrimString(app_name, "_", &app_name);
  return app_name;
}
#endif

}  // namespace web_app
