// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_util.h"

#include <string>

#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace {

// A helper method for adding a query param to |url|.
GURL AppendParam(const GURL& url,
                 const std::string& param_name,
                 const std::string& param_value) {
  std::string query(url.query());
  if (!query.empty())
    query += "&";
  query += param_name + "=" + param_value;
  GURL::Replacements repl;
  repl.SetQueryStr(query);
  return url.ReplaceComponents(repl);
}

}  // anonymous namespace

namespace google_util {

const char kLinkDoctorBaseURL[] =
    "http://linkhelp.clients.google.com/tbproxy/lh/fixurl";

GURL AppendGoogleLocaleParam(const GURL& url) {
  // Google does not yet recognize 'nb' for Norwegian Bokmal, but it uses
  // 'no' for that.
  std::string locale = g_browser_process->GetApplicationLocale();
  if (locale == "nb")
    locale = "no";
  return AppendParam(url, "hl", locale);
}

std::string StringAppendGoogleLocaleParam(const std::string& url) {
  GURL original_url(url);
  DCHECK(original_url.is_valid());
  GURL localized_url = AppendGoogleLocaleParam(original_url);
  return localized_url.spec();
}

GURL AppendGoogleTLDParam(const GURL& url) {
  const std::string google_domain(
      net::RegistryControlledDomainService::GetDomainAndRegistry(
          GoogleURLTracker::GoogleURL()));
  const size_t first_dot = google_domain.find('.');
  if (first_dot == std::string::npos) {
    NOTREACHED();
    return url;
  }
  return AppendParam(url, "sd", google_domain.substr(first_dot + 1));
}

}  // namespace google_util
