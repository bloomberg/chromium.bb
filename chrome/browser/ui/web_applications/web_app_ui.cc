// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/environment.h"
#endif

#if defined(OS_WIN)
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#endif

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;

namespace {

#if defined(OS_MACOSX)
const int kDesiredSizes[] = {16, 32, 128, 256, 512};
#elif defined(OS_LINUX)
// Linux supports icons of any size. FreeDesktop Icon Theme Specification states
// that "Minimally you should install a 48x48 icon in the hicolor theme."
const int kDesiredSizes[] = {16, 32, 48, 128, 256, 512};
#else
const int kDesiredSizes[] = {32};
#endif

#if defined(OS_WIN)
// UpdateShortcutWorker holds all context data needed for update shortcut.
// It schedules a pre-update check to find all shortcuts that needs to be
// updated. If there are such shortcuts, it schedules icon download and
// update them when icons are downloaded. It observes TAB_CLOSING notification
// and cancels all the work when the underlying tab is closing.
class UpdateShortcutWorker : public content::NotificationObserver {
 public:
  explicit UpdateShortcutWorker(WebContents* web_contents);

  void Run();

 private:
  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Downloads icon via the FaviconTabHelper.
  void DownloadIcon();

  // Favicon download callback.
  void DidDownloadFavicon(
      int id,
      const GURL& image_url,
      int requested_size,
      const std::vector<SkBitmap>& bitmaps);

  // Checks if shortcuts exists on desktop, start menu and quick launch.
  void CheckExistingShortcuts();

  // Update shortcut files and icons.
  void UpdateShortcuts();
  void UpdateShortcutsOnFileThread();

  // Callback after shortcuts are updated.
  void OnShortcutsUpdated(bool);

  // Deletes the worker on UI thread where it gets created.
  void DeleteMe();
  void DeleteMeOnUIThread();

  content::NotificationRegistrar registrar_;

  // Underlying WebContents whose shortcuts will be updated.
  WebContents* web_contents_;

  // Icons info from web_contents_'s web app data.
  web_app::IconInfoList unprocessed_icons_;

  // Cached shortcut data from the web_contents_.
  ShellIntegration::ShortcutInfo shortcut_info_;

  // Our copy of profile path.
  base::FilePath profile_path_;

  // File name of shortcut/ico file based on app title.
  base::FilePath file_name_;

  // Existing shortcuts.
  std::vector<base::FilePath> shortcut_files_;

  DISALLOW_COPY_AND_ASSIGN(UpdateShortcutWorker);
};

UpdateShortcutWorker::UpdateShortcutWorker(WebContents* web_contents)
    : web_contents_(web_contents),
      profile_path_(Profile::FromBrowserContext(
          web_contents->GetBrowserContext())->GetPath()) {
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(web_contents);
  web_app::GetShortcutInfoForTab(web_contents_, &shortcut_info_);
  web_app::GetIconsInfo(extensions_tab_helper->web_app_info(),
                        &unprocessed_icons_);
  file_name_ = web_app::internals::GetSanitizedFileName(shortcut_info_.title);

  registrar_.Add(
      this,
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&web_contents->GetController()));
}

void UpdateShortcutWorker::Run() {
  // Starting by downloading app icon.
  DownloadIcon();
}

void UpdateShortcutWorker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_TAB_CLOSING &&
      content::Source<NavigationController>(source).ptr() ==
        &web_contents_->GetController()) {
    // Underlying tab is closing.
    web_contents_ = NULL;
  }
}

void UpdateShortcutWorker::DownloadIcon() {
  // FetchIcon must run on UI thread because it relies on WebContents
  // to download the icon.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (web_contents_ == NULL) {
    DeleteMe();  // We are done if underlying WebContents is gone.
    return;
  }

  if (unprocessed_icons_.empty()) {
    // No app icon. Just use the favicon from WebContents.
    UpdateShortcuts();
    return;
  }

  web_contents_->DownloadFavicon(
      unprocessed_icons_.back().url,
      std::max(unprocessed_icons_.back().width,
               unprocessed_icons_.back().height),
      base::Bind(&UpdateShortcutWorker::DidDownloadFavicon,
                 base::Unretained(this)));
  unprocessed_icons_.pop_back();
}

void UpdateShortcutWorker::DidDownloadFavicon(
    int id,
    const GURL& image_url,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  std::vector<ui::ScaleFactor> scale_factors;
  scale_factors.push_back(ui::SCALE_FACTOR_100P);

  size_t closest_index =
      FaviconUtil::SelectBestFaviconFromBitmaps(bitmaps,
                                                scale_factors,
                                                requested_size);

  if (!bitmaps.empty() && !bitmaps[closest_index].isNull()) {
    // Update icon with download image and update shortcut.
    shortcut_info_.favicon =
        gfx::Image::CreateFrom1xBitmap(bitmaps[closest_index]);
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(web_contents_);
    extensions_tab_helper->SetAppIcon(bitmaps[closest_index]);
    UpdateShortcuts();
  } else {
    // Try the next icon otherwise.
    DownloadIcon();
  }
}

void UpdateShortcutWorker::CheckExistingShortcuts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Locations to check to shortcut_paths.
  struct {
    int location_id;
    const wchar_t* sub_dir;
  } locations[] = {
    {
      base::DIR_USER_DESKTOP,
      NULL
    }, {
      base::DIR_START_MENU,
      NULL
    }, {
      // For Win7, create_in_quick_launch_bar means pinning to taskbar.
      base::DIR_APP_DATA,
      (base::win::GetVersion() >= base::win::VERSION_WIN7) ?
          L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar" :
          L"Microsoft\\Internet Explorer\\Quick Launch"
    }
  };

  for (int i = 0; i < arraysize(locations); ++i) {
    base::FilePath path;
    if (!PathService::Get(locations[i].location_id, &path)) {
      NOTREACHED();
      continue;
    }

    if (locations[i].sub_dir != NULL)
      path = path.Append(locations[i].sub_dir);

    base::FilePath shortcut_file = path.Append(file_name_).
        ReplaceExtension(FILE_PATH_LITERAL(".lnk"));
    if (file_util::PathExists(shortcut_file)) {
      shortcut_files_.push_back(shortcut_file);
    }
  }
}

void UpdateShortcutWorker::UpdateShortcuts() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&UpdateShortcutWorker::UpdateShortcutsOnFileThread,
                 base::Unretained(this)));
}

void UpdateShortcutWorker::UpdateShortcutsOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath web_app_path = web_app::GetWebAppDataDirectory(
      profile_path_, shortcut_info_.extension_id, shortcut_info_.url);

  // Ensure web_app_path exists. web_app_path could be missing for a legacy
  // shortcut created by Gears.
  if (!file_util::PathExists(web_app_path) &&
      !file_util::CreateDirectory(web_app_path)) {
    NOTREACHED();
    return;
  }

  base::FilePath icon_file = web_app_path.Append(file_name_).ReplaceExtension(
      FILE_PATH_LITERAL(".ico"));
  web_app::internals::CheckAndSaveIcon(icon_file,
                                       *shortcut_info_.favicon.ToSkBitmap());

  // Update existing shortcuts' description, icon and app id.
  CheckExistingShortcuts();
  if (!shortcut_files_.empty()) {
    // Generates app id from web app url and profile path.
    string16 app_id = ShellIntegration::GetAppModelIdForProfile(
        UTF8ToWide(web_app::GenerateApplicationNameFromURL(shortcut_info_.url)),
        profile_path_);

    // Sanitize description
    if (shortcut_info_.description.length() >= MAX_PATH)
      shortcut_info_.description.resize(MAX_PATH - 1);

    for (size_t i = 0; i < shortcut_files_.size(); ++i) {
      base::win::ShortcutProperties shortcut_properties;
      shortcut_properties.set_target(shortcut_files_[i]);
      shortcut_properties.set_description(shortcut_info_.description);
      shortcut_properties.set_icon(icon_file, 0);
      shortcut_properties.set_app_id(app_id);
      base::win::CreateOrUpdateShortcutLink(
          shortcut_files_[i], shortcut_properties,
          base::win::SHORTCUT_UPDATE_EXISTING);
    }
  }

  OnShortcutsUpdated(true);
}

void UpdateShortcutWorker::OnShortcutsUpdated(bool) {
  DeleteMe();  // We are done.
}

void UpdateShortcutWorker::DeleteMe() {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DeleteMeOnUIThread();
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdateShortcutWorker::DeleteMeOnUIThread,
                 base::Unretained(this)));
  }
}

void UpdateShortcutWorker::DeleteMeOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delete this;
}
#endif  // defined(OS_WIN)

void OnImageLoaded(ShellIntegration::ShortcutInfo shortcut_info,
                   web_app::ShortcutInfoCallback callback,
                   const gfx::Image& image) {
  // If the image failed to load (e.g. if the resource being loaded was empty)
  // use the standard application icon.
  if (image.IsEmpty()) {
    gfx::Image default_icon =
        ResourceBundle::GetSharedInstance().GetImageNamed(IDR_APP_DEFAULT_ICON);
    int size = kDesiredSizes[arraysize(kDesiredSizes) - 1];
    SkBitmap bmp = skia::ImageOperations::Resize(
          *default_icon.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST,
          size, size);
    shortcut_info.favicon = gfx::Image::CreateFrom1xBitmap(bmp);
  } else {
    shortcut_info.favicon = image;
  }

  callback.Run(shortcut_info);
}

}  // namespace

namespace web_app {

ShellIntegration::ShortcutInfo ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* extension, Profile* profile) {
  ShellIntegration::ShortcutInfo shortcut_info;
  web_app::UpdateShortcutInfoForApp(*extension, profile, &shortcut_info);
  return shortcut_info;
}

void GetShortcutInfoForTab(WebContents* web_contents,
                           ShellIntegration::ShortcutInfo* info) {
  DCHECK(info);  // Must provide a valid info.

  const FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(web_contents);
  const extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(web_contents);
  const WebApplicationInfo& app_info = extensions_tab_helper->web_app_info();

  info->url = app_info.app_url.is_empty() ? web_contents->GetURL() :
                                            app_info.app_url;
  info->title = app_info.title.empty() ?
      (web_contents->GetTitle().empty() ? UTF8ToUTF16(info->url.spec()) :
                                          web_contents->GetTitle()) :
      app_info.title;
  info->description = app_info.description;
  info->favicon = gfx::Image(favicon_tab_helper->GetFavicon());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  info->profile_path = profile->GetPath();
}

void UpdateShortcutForTabContents(WebContents* web_contents) {
#if defined(OS_WIN)
  // UpdateShortcutWorker will delete itself when it's done.
  UpdateShortcutWorker* worker = new UpdateShortcutWorker(web_contents);
  worker->Run();
#endif  // defined(OS_WIN)
}

void UpdateShortcutInfoForApp(const extensions::Extension& app,
                              Profile* profile,
                              ShellIntegration::ShortcutInfo* shortcut_info) {
  shortcut_info->extension_id = app.id();
  shortcut_info->is_platform_app = app.is_platform_app();
  shortcut_info->url = GURL(app.launch_web_url());
  shortcut_info->title = UTF8ToUTF16(app.name());
  shortcut_info->description = UTF8ToUTF16(app.description());
  shortcut_info->extension_path = app.path();
  shortcut_info->profile_path = profile->GetPath();
}

void UpdateShortcutInfoAndIconForApp(
    const extensions::Extension& extension,
    Profile* profile,
    const web_app::ShortcutInfoCallback& callback) {
  ShellIntegration::ShortcutInfo shortcut_info =
      ShortcutInfoForExtensionAndProfile(&extension, profile);

  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < arraysize(kDesiredSizes); ++i) {
    int size = kDesiredSizes[i];
    ExtensionResource resource = extensions::IconsInfo::GetIconResource(
        &extension, size, ExtensionIconSet::MATCH_EXACTLY);
    if (!resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
          resource,
          extensions::ImageLoader::ImageRepresentation::RESIZE_WHEN_LARGER,
          gfx::Size(size, size),
          ui::SCALE_FACTOR_100P));
    }
  }

  if (info_list.empty()) {
    size_t i = arraysize(kDesiredSizes) - 1;
    int size = kDesiredSizes[i];

    // If there is no icon at the desired sizes, we will resize what we can get.
    // Making a large icon smaller is preferred to making a small icon larger,
    // so look for a larger icon first:
    ExtensionResource resource = extensions::IconsInfo::GetIconResource(
        &extension, size, ExtensionIconSet::MATCH_BIGGER);
    if (resource.empty()) {
      resource = extensions::IconsInfo::GetIconResource(
          &extension, size, ExtensionIconSet::MATCH_SMALLER);
    }
    info_list.push_back(extensions::ImageLoader::ImageRepresentation(
        resource,
        extensions::ImageLoader::ImageRepresentation::RESIZE_WHEN_LARGER,
        gfx::Size(size, size),
        ui::SCALE_FACTOR_100P));
  }

  // |info_list| may still be empty at this point, in which case LoadImage
  // will call the OnImageLoaded callback with an empty image and exit
  // immediately.
  extensions::ImageLoader::Get(profile)->LoadImagesAsync(&extension, info_list,
      base::Bind(&OnImageLoaded, shortcut_info, callback));
}

}  // namespace web_app
