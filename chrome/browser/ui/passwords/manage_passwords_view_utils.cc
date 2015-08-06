// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/secure_display/elide_url.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

const int kAvatarImageSize = 50;

gfx::ImageSkia ScaleImageForAccountAvatar(gfx::ImageSkia skia_image) {
  gfx::Size size = skia_image.size();
  if (size.height() != size.width()) {
    gfx::Rect target(size);
    int side = std::min(size.height(), size.width());
    target.ClampToCenteredSize(gfx::Size(side, side));
    skia_image = gfx::ImageSkiaOperations::ExtractSubset(skia_image, target);
  }
  return gfx::ImageSkiaOperations::CreateResizedImage(
      skia_image,
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kAvatarImageSize, kAvatarImageSize));
}

void GetSavePasswordDialogTitleTextAndLinkRange(
    const GURL& user_visible_url,
    const GURL& form_origin_url,
    bool is_smartlock_branding_enabled,
    bool is_update_password_bubble,
    base::string16* title,
    gfx::Range* title_link_range) {
  std::vector<size_t> offsets;
  std::vector<base::string16> replacements;
  int title_id =
      is_update_password_bubble ? IDS_UPDATE_PASSWORD : IDS_SAVE_PASSWORD;

  // Check whether the registry controlled domains for user-visible URL (i.e.
  // the one seen in the omnibox) and the password form post-submit navigation
  // URL differs or not.
  bool target_domain_differs =
      !net::registry_controlled_domains::SameDomainOrHost(
          user_visible_url, form_origin_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (target_domain_differs) {
    title_id = IDS_SAVE_PASSWORD_TITLE;
    replacements.push_back(secure_display::FormatUrlForSecurityDisplay(
        form_origin_url, std::string()));
  }

  if (is_smartlock_branding_enabled) {
    // "Google Smart Lock" should be a hyperlink.
    base::string16 title_link =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SMART_LOCK);
    replacements.insert(replacements.begin(), title_link);
    *title = l10n_util::GetStringFUTF16(title_id, replacements, &offsets);
    *title_link_range =
        gfx::Range(offsets[0], offsets[0] + title_link.length());
  } else {
    replacements.insert(
        replacements.begin(),
        l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_TITLE_BRAND));
    *title = l10n_util::GetStringFUTF16(title_id, replacements, &offsets);
  }
}
