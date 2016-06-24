// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/gfx_utils.h"

#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace extensions {
namespace util {

bool MaybeApplyChromeBadge(content::BrowserContext* context,
                           const std::string& extension_id,
                           gfx::ImageSkia* icon_out) {
  DCHECK(context);
  DCHECK(icon_out);

  Profile* profile = Profile::FromBrowserContext(context);
  // Only apply Chrome badge for the primary profile.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile) ||
      !multi_user_util::IsProfileFromActiveUser(profile)) {
    return false;
  }

  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  if (!arc_auth_service ||
      arc_auth_service->state() != arc::ArcAuthService::State::ACTIVE) {
    return false;
  }

  const ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  if (!registry)
    return false;

  const Extension* extension = registry->GetInstalledExtension(extension_id);
  if (!extension || !extension->is_hosted_app())
    return false;

  const gfx::ImageSkia* badge_image =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_ARC_DUAL_ICON_BADGE);
  DCHECK(badge_image);

  gfx::ImageSkia resized_badge_image = *badge_image;
  if (badge_image->size() != icon_out->size()) {
    resized_badge_image = gfx::ImageSkiaOperations::CreateResizedImage(
        *badge_image, skia::ImageOperations::RESIZE_BEST, icon_out->size());
  }
  *icon_out = gfx::ImageSkiaOperations::CreateSuperimposedImage(
      *icon_out, resized_badge_image);
  return true;
}

}  // namespace util
}  // namespace extensions
