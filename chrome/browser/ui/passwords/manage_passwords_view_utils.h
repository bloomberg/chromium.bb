// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_

#include "base/strings/string16.h"

namespace autofill {
struct PasswordForm;
}

namespace gfx {
class ImageSkia;
class Range;
}  // namespace gfx

class GURL;

enum class PasswordTittleType {
  SAVE_PASSWORD,    // plain password
  SAVE_ACCOUNT,     // login via IDP
  UPDATE_PASSWORD,  // update plain password
};

// The desired width and height in pixels for an account avatar.
extern const int kAvatarImageSize;

// Crops and scales |image_skia| to the desired size for an account avatar.
gfx::ImageSkia ScaleImageForAccountAvatar(gfx::ImageSkia image_skia);

// Sets the formatted |title| in the Save Password bubble or the Update Password
// bubble (depending on |dialog_type|). If the registry controlled domain of
// |user_visible_url| (i.e. the one seen in the omnibox) differs from the
// registry controlled domain of |form_origin_url|, sets
// |IDS_SAVE_PASSWORD_TITLE| as the |title| so that it replaces "this site" in
// title text with output of |FormatUrlForSecurityDisplay(form_origin_url)|.
// Otherwise, sets |IDS_SAVE_PASSWORD| as the |title| having "this site".
// If |is_smartlock_branding_enabled| is true, sets the |title_link_range| for
// the "Google Smart Lock" text range to be set visibly as a hyperlink in the
// dialog bubble.
void GetSavePasswordDialogTitleTextAndLinkRange(
    const GURL& user_visible_url,
    const GURL& form_origin_url,
    bool is_smartlock_branding_enabled,
    PasswordTittleType dialog_type,
    base::string16* title,
    gfx::Range* title_link_range);

// Sets the formatted |title| in the Manage Passwords bubble. If the registry
// controlled domain of |user_visible_url| (i.e. the one seen in the omnibox)
// differs from the domain of the managed password origin URL
// |password_origin_url|, sets |IDS_MANAGE_PASSWORDS_TITLE_DIFFERENT_DOMAIN| as
// the |title| so that it replaces "this site" in title text with output of
// |FormatUrlForSecurityDisplay(password_origin_url)|.
// Otherwise, sets |IDS_MANAGE_PASSWORDS_TITLE| as the |title| having
// "this site".
void GetManagePasswordsDialogTitleText(const GURL& user_visible_url,
                                       const GURL& password_origin_url,
                                       base::string16* title);

// Sets the formatted |title| in the Account Chooser UI.
// If |is_smartlock_branding_enabled| is true, sets the |title_link_range| for
// the "Google Smart Lock" text range to be set visibly as a hyperlink in the
// dialog bubble otherwise chooses the title which doesn't contain Smart Lock
// branding.
void GetAccountChooserDialogTitleTextAndLinkRange(
    bool is_smartlock_branding_enabled,
    base::string16* title,
    gfx::Range* title_link_range);

// Loads |smartlock_string_id| or |default_string_id| string from the resources
// and substitutes the placeholder with the correct password manager branding
// (Google Smart Lock, Google Chrome or Chromium) according to
// |is_smartlock_branding_enabled|. If |is_smartlock_branding_enabled| is true
// then |link_range| contains the link range for the brand name.
void GetBrandedTextAndLinkRange(
    bool is_smartlock_branding_enabled,
    int smartlock_string_id,
    int default_string_id,
    base::string16* out_string,
    gfx::Range* link_range);

// Returns an username in the form that should be shown in the bubble.
base::string16 GetDisplayUsername(const autofill::PasswordForm& form);

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_
