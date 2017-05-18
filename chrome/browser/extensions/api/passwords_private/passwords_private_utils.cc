// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include "base/strings/string_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kAndroidAppScheme[] = "android://";
constexpr char kPlayStoreAppPrefix[] =
    "https://play.google.com/store/apps/details?id=";

}  // namespace

namespace extensions {

api::passwords_private::UrlCollection CreateUrlCollectionFromForm(
    const autofill::PasswordForm& form) {
  bool is_android_uri = false;
  GURL link_url;
  bool origin_is_clickable = false;
  api::passwords_private::UrlCollection urls;

  urls.origin = form.signon_realm;
  urls.shown = password_manager::GetShownOriginAndLinkUrl(
      form, &is_android_uri, &link_url, &origin_is_clickable);
  urls.link = link_url.spec();

  if (is_android_uri) {
    if (!origin_is_clickable) {
      // If the origin is not clickable, link to the PlayStore. Use that
      // |urls.shown| is the result of |GetHumanReadableOriginForAndroidUri| and
      // does not contain the base64 hash anymore.
      urls.link = urls.shown;
      // e.g. android://com.example.r => r.example.com.
      urls.shown = password_manager::StripAndroidAndReverse(urls.shown);
      // Turn human unfriendly string into a clickable link to the PlayStore.
      base::ReplaceFirstSubstringAfterOffset(&urls.link, 0, kAndroidAppScheme,
                                             kPlayStoreAppPrefix);
    }

    // Currently we use "direction=rtl" in CSS to elide long origins from the
    // left. This does not play nice with appending strings that end in
    // punctuation symbols, which is why the bidirectional override tag is
    // necessary.
    // TODO(crbug.com/679434): Clean this up.
    // Reference:
    // https://www.w3.org/International/questions/qa-bidi-unicode-controls
    urls.shown += "\u202D" +  // equivalent to <bdo dir = "ltr">
                  l10n_util::GetStringUTF8(IDS_PASSWORDS_ANDROID_URI_SUFFIX) +
                  "\u202C";  // equivalent to </bdo>
  }

  return urls;
}

}  // namespace extensions
