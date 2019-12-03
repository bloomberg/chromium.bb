// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace {

// Upper bound on length of selection text to avoid performance issues. Take
// into account numbers with country code and spaces '+99 0 999 999 9999' when
// reducing the max length.
constexpr int kSelectionTextMaxLength = 30;

bool IsClickToCallEnabled(content::BrowserContext* browser_context) {
  // Check Chrome enterprise policy for Click to Call.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && !profile->GetPrefs()->GetBoolean(prefs::kClickToCallEnabled))
    return false;

  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(browser_context);
  return sharing_service && base::FeatureList::IsEnabled(kClickToCallUI);
}

}  // namespace

bool ShouldOfferClickToCallForURL(content::BrowserContext* browser_context,
                                  const GURL& url) {
  return !url.is_empty() && url.SchemeIs(url::kTelScheme) &&
         !url.GetContent().empty() && IsClickToCallEnabled(browser_context);
}

base::Optional<std::string> ExtractPhoneNumberForClickToCall(
    content::BrowserContext* browser_context,
    const std::string& selection_text) {
  DCHECK(!selection_text.empty());

  if (selection_text.size() > kSelectionTextMaxLength)
    return base::nullopt;

  if (!IsClickToCallEnabled(browser_context))
    return base::nullopt;

  LogPhoneNumberDetectionMetrics(selection_text, /*sent_to_device=*/false);

  if (base::FeatureList::IsEnabled(kClickToCallDetectionV2)) {
    return ExtractPhoneNumber(selection_text,
                              PhoneNumberRegexVariant::kLowConfidenceModified);
  }

  return ExtractPhoneNumber(selection_text, PhoneNumberRegexVariant::kSimple);
}

base::Optional<std::string> ExtractPhoneNumber(
    const std::string& selection_text,
    PhoneNumberRegexVariant regex_variant) {
  ScopedUmaHistogramMicrosecondsTimer scoped_uma_timer(regex_variant);
  std::string parsed_number;

  const re2::RE2& regex = GetPhoneNumberRegex(regex_variant);
  if (!re2::RE2::PartialMatch(selection_text, regex, &parsed_number))
    return base::nullopt;

  return base::UTF16ToUTF8(
      base::TrimWhitespace(base::UTF8ToUTF16(parsed_number), base::TRIM_ALL));
}

std::string GetUnescapedURLContent(const GURL& url) {
  std::string content_string(url.GetContent());
  url::RawCanonOutputT<base::char16> unescaped_content;
  url::DecodeURLEscapeSequences(content_string.data(), content_string.size(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped_content);
  return base::UTF16ToUTF8(
      base::string16(unescaped_content.data(), unescaped_content.length()));
}
