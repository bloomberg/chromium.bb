// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list/extension_app_item.h"

#include "ash/app_list/app_list_item_view.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/component_extension_resources_map.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace {

const ExtensionSorting* GetExtensionSorting(Profile* profile) {
  return profile->GetExtensionService()->extension_prefs()->extension_sorting();
}

// Gets page ordinal of given |extension| in its app launch page.
StringOrdinal GetExtensionPageOrdinal(Profile* profile,
                                      const Extension* extension) {
  const ExtensionSorting* sorting = GetExtensionSorting(profile);
  StringOrdinal page_ordinal = sorting->GetPageOrdinal(extension->id());
  if (!page_ordinal.IsValid()) {
    page_ordinal = extension->id() == extension_misc::kWebStoreAppId ?
        sorting->CreateFirstAppPageOrdinal() :
        sorting->GetNaturalAppPageOrdinal();
  }

  return page_ordinal;
}

// Gets page index from page ordinal.
int GetExtensionPageIndex(Profile* profile,
                          const Extension* extension) {
  return std::max(0, GetExtensionSorting(profile)->PageStringOrdinalAsInteger(
      GetExtensionPageOrdinal(profile, extension)));
}

// Gets app launch ordinal of given extension.
StringOrdinal GetExtensionLaunchOrdinal(Profile* profile,
                                        const Extension* extension) {
  const ExtensionSorting* sorting = GetExtensionSorting(profile);
  StringOrdinal app_launch_ordinal =
      sorting->GetAppLaunchOrdinal(extension->id());
  if (!app_launch_ordinal.IsValid()) {
    StringOrdinal page_ordinal = GetExtensionPageOrdinal(profile, extension);
    app_launch_ordinal = extension->id() == extension_misc::kWebStoreAppId ?
        sorting->CreateFirstAppLaunchOrdinal(page_ordinal) :
        sorting->CreateNextAppLaunchOrdinal(page_ordinal);
  }
  return app_launch_ordinal;
}

}

ExtensionAppItem::ExtensionAppItem(Profile* profile,
                                   const Extension* extension)
    : profile_(profile),
      extension_id_(extension->id()),
      page_index_(GetExtensionPageIndex(profile, extension)),
      launch_ordinal_(GetExtensionLaunchOrdinal(profile, extension)) {
  SetTitle(extension->name());
  LoadImage(extension);
}

ExtensionAppItem::~ExtensionAppItem() {
}

const Extension* ExtensionAppItem::GetExtension() const {
  const Extension* extension =
    profile_->GetExtensionService()->GetInstalledExtension(extension_id_);
  return extension;
}

void ExtensionAppItem::LoadImage(const Extension* extension) {
  ExtensionResource icon = extension->GetIconResource(
      ash::AppListItemView::kIconSize,
      ExtensionIconSet::MATCH_BIGGER);
  if (icon.relative_path().empty()) {
    LoadDefaultImage();
    return;
  }

  if (extension->location() == Extension::COMPONENT) {
    FilePath directory_path = extension->path();
    FilePath relative_path = directory_path.BaseName().Append(
        icon.relative_path());
    for (size_t i = 0; i < kComponentExtensionResourcesSize; ++i) {
      FilePath bm_resource_path =
          FilePath().AppendASCII(kComponentExtensionResources[i].name);
#if defined(OS_WIN)
      bm_resource_path = bm_resource_path.NormalizeWindowsPathSeparators();
#endif
      if (relative_path == bm_resource_path) {
        ResourceBundle& rb = ResourceBundle::GetSharedInstance();
        int resource = kComponentExtensionResources[i].value;

        base::StringPiece contents = rb.GetRawDataResource(resource);
        SkBitmap icon;
        if (gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(contents.data()),
            contents.size(), &icon)) {
          SetIcon(icon);
          return;
        } else {
          NOTREACHED() << "Unable to decode image resource " << resource;
        }
      }
    }
  }

  tracker_.reset(new ImageLoadingTracker(this));
  tracker_->LoadImage(extension,
                      icon,
                      gfx::Size(ash::AppListItemView::kIconSize,
                                ash::AppListItemView::kIconSize),
                      ImageLoadingTracker::DONT_CACHE);
}

void ExtensionAppItem::LoadDefaultImage() {
  const Extension* extension = GetExtension();
  int resource = IDR_APP_DEFAULT_ICON;
  if (extension && extension->id() == extension_misc::kWebStoreAppId)
      resource = IDR_WEBSTORE_ICON;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetImageNamed(resource).ToSkBitmap());
}

void ExtensionAppItem::OnImageLoaded(SkBitmap* image,
                                     const ExtensionResource& resource,
                                     int tracker_index) {
  if (image && !image->empty())
    SetIcon(*image);
  else
    LoadDefaultImage();
}

void ExtensionAppItem::Activate(int event_flags) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  WindowOpenDisposition disposition =
      browser::DispositionFromEventFlags(event_flags);

  GURL url;
  if (extension_id_ == extension_misc::kWebStoreAppId)
    url = extension->GetFullLaunchURL();

  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB) {
    // Opens in a tab.
    Browser::OpenApplication(
        profile_, extension, extension_misc::LAUNCH_TAB, url, disposition);
  } else if (disposition == NEW_WINDOW) {
    // Force a new window open.
    Browser::OpenApplication(
        profile_, extension, extension_misc::LAUNCH_WINDOW, url,
        disposition);
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    extension_misc::LaunchContainer launch_container =
        profile_->GetExtensionService()->extension_prefs()->GetLaunchContainer(
            extension, ExtensionPrefs::LAUNCH_REGULAR);

    Browser::OpenApplication(
        profile_, extension, launch_container, GURL(url),
        NEW_FOREGROUND_TAB);
  }
}
