// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/safe_search_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace {

const char kYouTubePrefCookieName[] = "PREF";
// YouTube pref flags are stored in bit masks of 31 bits each, called "f1",
// "f2" etc. The Safety Mode flag is bit 58, so bit 27 in "f2".
const char kYouTubePrefCookieSafetyModeFlagsEntryName[] = "f2";
const int kYouTubePrefCookieSafetyModeFlagsEntryValue = (1 << 27);

// Returns whether a URL parameter, |first_parameter| (e.g. foo=bar), has the
// same key as the the |second_parameter| (e.g. foo=baz). Both parameters
// must be in key=value form.
bool HasSameParameterKey(const std::string& first_parameter,
                         const std::string& second_parameter) {
  DCHECK(second_parameter.find("=") != std::string::npos);
  // Prefix for "foo=bar" is "foo=".
  std::string parameter_prefix = second_parameter.substr(
      0, second_parameter.find("=") + 1);
  return StartsWithASCII(first_parameter, parameter_prefix, false);
}

// Examines the query string containing parameters and adds the necessary ones
// so that SafeSearch is active. |query| is the string to examine and the
// return value is the |query| string modified such that SafeSearch is active.
std::string AddSafeSearchParameters(const std::string& query) {
  std::vector<std::string> new_parameters;
  std::string safe_parameter = chrome::kSafeSearchSafeParameter;
  std::string ssui_parameter = chrome::kSafeSearchSsuiParameter;

  std::vector<std::string> parameters;
  base::SplitString(query, '&', &parameters);

  std::vector<std::string>::iterator it;
  for (it = parameters.begin(); it < parameters.end(); ++it) {
    if (!HasSameParameterKey(*it, safe_parameter) &&
        !HasSameParameterKey(*it, ssui_parameter)) {
      new_parameters.push_back(*it);
    }
  }

  new_parameters.push_back(safe_parameter);
  new_parameters.push_back(ssui_parameter);
  return JoinString(new_parameters, '&');
}

bool IsYouTubePrefCookie(const net::cookie_util::ParsedRequestCookie& cookie) {
  return cookie.first == base::StringPiece(kYouTubePrefCookieName);
}

bool IsYouTubePrefCookieSafetyModeFlagsEntry(
    const std::pair<std::string, std::string>& pref_entry) {
  return pref_entry.first == kYouTubePrefCookieSafetyModeFlagsEntryName;
}

std::string JoinStringKeyValuePair(
    const base::StringPairs::value_type& key_value,
    char delimiter) {
  return key_value.first + delimiter + key_value.second;
}

// Does the opposite of base::SplitStringIntoKeyValuePairs() from
// base/strings/string_util.h.
std::string JoinStringKeyValuePairs(const base::StringPairs& pairs,
                                    char key_value_delimiter,
                                    char key_value_pair_delimiter) {
  if (pairs.empty())
    return std::string();

  base::StringPairs::const_iterator it = pairs.begin();
  std::string result = JoinStringKeyValuePair(*it, key_value_delimiter);
  ++it;

  for (; it != pairs.end(); ++it) {
    result += key_value_pair_delimiter;
    result += JoinStringKeyValuePair(*it, key_value_delimiter);
  }

  return result;
}

} // namespace

namespace safe_search_util {

// If |request| is a request to Google Web Search the function
// enforces that the SafeSearch query parameters are set to active.
// Sets the query part of |new_url| with the new value of the parameters.
void ForceGoogleSafeSearch(const net::URLRequest* request, GURL* new_url) {
  if (!google_util::IsGoogleSearchUrl(request->url()) &&
      !google_util::IsGoogleHomePageUrl(request->url()))
    return;

  std::string query = request->url().query();
  std::string new_query = AddSafeSearchParameters(query);
  if (query == new_query)
    return;

  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query);
  *new_url = request->url().ReplaceComponents(replacements);
}

// If |request| is a request to YouTube, enforces YouTube's Safety Mode by
// adding/modifying YouTube's PrefCookie header.
void ForceYouTubeSafetyMode(const net::URLRequest* request,
                            net::HttpRequestHeaders* headers) {
  if (!google_util::IsYoutubeDomainUrl(
          request->url(),
          google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS))
    return;

  // Get the cookie string from the headers and parse it into key/value pairs.
  std::string cookie_string;
  headers->GetHeader(base::StringPiece(net::HttpRequestHeaders::kCookie),
                     &cookie_string);
  net::cookie_util::ParsedRequestCookies cookies;
  net::cookie_util::ParseRequestCookieLine(cookie_string, &cookies);

  // Find YouTube's pref cookie, or add it if it doesn't exist yet.
  net::cookie_util::ParsedRequestCookies::iterator pref_it =
      std::find_if(cookies.begin(), cookies.end(), IsYouTubePrefCookie);
  if (pref_it == cookies.end()) {
    cookies.push_back(std::make_pair(base::StringPiece(kYouTubePrefCookieName),
                                     base::StringPiece()));
    pref_it = cookies.end() - 1;
  }

  // The pref cookie's value may be quoted. If so, remove the quotes.
  std::string pref_string = pref_it->second.as_string();
  bool pref_string_quoted = false;
  if (pref_string.size() >= 2 &&
      pref_string[0] == '\"' &&
      pref_string[pref_string.size() - 1] == '\"') {
    pref_string_quoted = true;
    pref_string = pref_string.substr(1, pref_string.length() - 2);
  }

  // The pref cookie's value consists of key/value pairs. Parse them.
  base::StringPairs pref_values;
  base::SplitStringIntoKeyValuePairs(pref_string, '=', '&', &pref_values);

  // Find the "flags" entry that contains the Safety Mode flag, or add it if it
  // doesn't exist.
  base::StringPairs::iterator flag_it =
      std::find_if(pref_values.begin(), pref_values.end(),
                   IsYouTubePrefCookieSafetyModeFlagsEntry);
  int flag_value = 0;
  if (flag_it == pref_values.end()) {
    pref_values.push_back(
        std::make_pair(std::string(kYouTubePrefCookieSafetyModeFlagsEntryName),
                       std::string()));
    flag_it = pref_values.end() - 1;
  } else {
    base::HexStringToInt(base::StringPiece(flag_it->second), &flag_value);
  }

  // Set the Safety Mode bit.
  flag_value |= kYouTubePrefCookieSafetyModeFlagsEntryValue;

  // Finally, put it all back together and replace the original cookie string.
  flag_it->second = base::StringPrintf("%x", flag_value);
  pref_string = JoinStringKeyValuePairs(pref_values, '=', '&');
  if (pref_string_quoted) {
    pref_string = '\"' + pref_string + '\"';
  }
  pref_it->second = base::StringPiece(pref_string);
  cookie_string = net::cookie_util::SerializeRequestCookieLine(cookies);
  headers->SetHeader(base::StringPiece(net::HttpRequestHeaders::kCookie),
                     base::StringPiece(cookie_string));
}

}  // namespace safe_search_util
