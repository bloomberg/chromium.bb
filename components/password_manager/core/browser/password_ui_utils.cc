// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_ui_utils.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/url_formatter/elide_url.h"

namespace password_manager {

namespace {

// The URL prefixes that are removed from shown origin.
const char* const kRemovedPrefixes[] = {"m.", "mobile.", "www."};

}  // namespace

std::string GetShownOrigin(const autofill::PasswordForm& password_form,
                           const std::string& languages,
                           bool* is_android_uri) {
  DCHECK(is_android_uri != nullptr);

  password_manager::FacetURI facet_uri =
      password_manager::FacetURI::FromPotentiallyInvalidSpec(
          password_form.signon_realm);
  *is_android_uri = facet_uri.IsValidAndroidFacetURI();
  if (*is_android_uri)
    return GetHumanReadableOriginForAndroidUri(facet_uri);

  return GetShownOrigin(password_form.origin, languages);
}

std::string GetShownOrigin(const GURL& origin, const std::string& languages) {
  std::string original = base::UTF16ToUTF8(
      url_formatter::FormatUrlForSecurityDisplayOmitScheme(origin, languages));
  base::StringPiece result = original;
  for (base::StringPiece prefix : kRemovedPrefixes) {
    if (base::StartsWith(result, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      result.remove_prefix(prefix.length());
      break;  // Remove only one prefix (e.g. www.mobile.de).
    }
  }

  return result.find('.') != base::StringPiece::npos ? result.as_string()
                                                     : original;
}

}  // namespace password_manager
