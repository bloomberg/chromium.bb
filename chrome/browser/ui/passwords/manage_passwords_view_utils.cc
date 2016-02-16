// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"

#include <stddef.h>

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/url_formatter/elide_url.h"
#include "grit/components_strings.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace {

// Checks whether two URLs are from the same domain or host.
bool SameDomainOrHost(const GURL& gurl1, const GURL& gurl2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      gurl1, gurl2,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

const int kAvatarImageSize = 32;

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

std::pair<base::string16, base::string16> GetCredentialLabelsForAccountChooser(
    const autofill::PasswordForm& form) {
  const base::string16& upper_string =
      form.display_name.empty() ? form.username_value : form.display_name;
  base::string16 lower_string;
  if (form.federation_url.is_empty()) {
    if (!form.display_name.empty())
      lower_string = form.username_value;
  } else {
    lower_string = l10n_util::GetStringFUTF16(
        IDS_PASSWORDS_VIA_FEDERATION,
        base::UTF8ToUTF16(form.federation_url.host()));
  }
  return std::make_pair(upper_string, lower_string);
}

void GetSavePasswordDialogTitleTextAndLinkRange(
    const GURL& user_visible_url,
    const GURL& form_origin_url,
    bool is_smartlock_branding_enabled,
    PasswordTittleType dialog_type,
    base::string16* title,
    gfx::Range* title_link_range) {
  std::vector<size_t> offsets;
  std::vector<base::string16> replacements;
  int title_id = 0;
  switch (dialog_type) {
    case PasswordTittleType::SAVE_PASSWORD:
      title_id = IDS_SAVE_PASSWORD;
      break;
    case PasswordTittleType::SAVE_ACCOUNT:
      title_id = IDS_SAVE_ACCOUNT;
      break;
    case PasswordTittleType::UPDATE_PASSWORD:
      title_id = IDS_UPDATE_PASSWORD;
      break;
  }

  // Check whether the registry controlled domains for user-visible URL (i.e.
  // the one seen in the omnibox) and the password form post-submit navigation
  // URL differs or not.
  if (!SameDomainOrHost(user_visible_url, form_origin_url)) {
    title_id = IDS_SAVE_PASSWORD_TITLE;
    // TODO(palmer): Look into passing real language prefs here, not "".
    // crbug.com/498069.
    replacements.push_back(url_formatter::FormatUrlForSecurityDisplay(
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
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TITLE_BRAND));
    *title = l10n_util::GetStringFUTF16(title_id, replacements, &offsets);
  }
}

void GetManagePasswordsDialogTitleText(const GURL& user_visible_url,
                                       const GURL& password_origin_url,
                                       base::string16* title) {
  // Check whether the registry controlled domains for user-visible URL
  // (i.e. the one seen in the omnibox) and the managed password origin URL
  // differ or not.
  if (!SameDomainOrHost(user_visible_url, password_origin_url)) {
    // TODO(palmer): Look into passing real language prefs here, not "".
    base::string16 formatted_url = url_formatter::FormatUrlForSecurityDisplay(
        password_origin_url, std::string());
    *title = l10n_util::GetStringFUTF16(
        IDS_MANAGE_PASSWORDS_TITLE_DIFFERENT_DOMAIN, formatted_url);
  } else {
    *title = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_TITLE);
  }
}

void GetAccountChooserDialogTitleTextAndLinkRange(
    bool is_smartlock_branding_enabled,
    base::string16* title,
    gfx::Range* title_link_range) {
  GetBrandedTextAndLinkRange(is_smartlock_branding_enabled,
                             IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_TITLE,
                             IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_TITLE,
                             title, title_link_range);
}

void GetBrandedTextAndLinkRange(bool is_smartlock_branding_enabled,
                                int smartlock_string_id,
                                int default_string_id,
                                base::string16* out_string,
                                gfx::Range* link_range) {
  if (is_smartlock_branding_enabled) {
    size_t offset;
    base::string16 brand_name =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SMART_LOCK);
    *out_string = l10n_util::GetStringFUTF16(smartlock_string_id, brand_name,
                                             &offset);
    *link_range = gfx::Range(offset, offset + brand_name.length());
  } else {
    *out_string = l10n_util::GetStringFUTF16(
        default_string_id,
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TITLE_BRAND));
    *link_range = gfx::Range();
  }
}

base::string16 GetDisplayUsername(const autofill::PasswordForm& form) {
  return form.username_value.empty()
             ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             : form.username_value;
}
