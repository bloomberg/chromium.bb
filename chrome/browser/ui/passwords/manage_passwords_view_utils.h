// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_

#include "base/strings/string16.h"

namespace gfx {
class ImageSkia;
class Range;
}  // namespace gfx

class GURL;

// The desired width and height in pixels for an account avatar.
extern const int kAvatarImageSize;

// Crops and scales |image_skia| to the desired size for an account avatar.
gfx::ImageSkia ScaleImageForAccountAvatar(gfx::ImageSkia image_skia);

// Sets the formatted |title| in the Save Password bubble or the Update Password
// bubble (depending on |is_update_password_bubble|). If the registry
// controlled domain of |user_visible_url| (i.e. the one seen in the omnibox)
// differs from the registry controlled domain of |form_origin_url|, sets
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
    bool is_update_password_bubble,
    base::string16* title,
    gfx::Range* title_link_range);

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_H_
