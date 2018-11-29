// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/email_providers_list.h"

#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"
#include "chrome/grit/onboarding_welcome_resources.h"
#include "components/country_codes/country_codes.h"

namespace nux {

std::vector<BookmarkItem> GetCurrentCountryEmailProviders() {
  switch (country_codes::GetCurrentCountryID()) {
    case country_codes::CountryCharsToCountryID('U', 'S'): {
      return {
          {EmailProviders::kGmail, "Gmail", "gmail",
           "https://accounts.google.com/b/0/AddMailService",
           IDR_NUX_EMAIL_GMAIL_1X},
          {EmailProviders::kYahoo, "Yahoo", "yahoo", "https://mail.yahoo.com",
           IDR_NUX_EMAIL_YAHOO_1X},
          {EmailProviders::kOutlook, "Outlook", "outlook",
           "https://login.live.com/login.srf?", IDR_NUX_EMAIL_OUTLOOK_1X},
          {EmailProviders::kAol, "AOL", "aol", "https://mail.aol.com",
           IDR_NUX_EMAIL_AOL_1X},
          {EmailProviders::kiCloud, "iCloud", "icloud",
           "https://www.icloud.com/mail", IDR_NUX_EMAIL_ICLOUD_1X},
      };
    }

      // TODO(scottchen): define all supported countries here.

    default:
      return {
          {EmailProviders::kGmail, "Gmail", "gmail",
           "https://accounts.google.com/b/0/AddMailService",
           IDR_NUX_EMAIL_GMAIL_1X},
          // TODO(scottchen): add more default values here.
      };
  }
}

}  // namespace nux
