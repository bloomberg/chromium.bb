// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_util.h"

#include <string>

#include "base/command_line.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mac/keystone_glue.h"
#endif

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

#if defined(OS_WIN)

bool GetBrand(std::string* brand) {
  string16 brand16;
  bool ret = GoogleUpdateSettings::GetBrand(&brand16);
  if (ret)
    brand->assign(WideToASCII(brand16));
  return ret;
}

bool GetReactivationBrand(std::string* brand) {
  string16 brand16;
  bool ret = GoogleUpdateSettings::GetReactivationBrand(&brand16);
  if (ret)
    brand->assign(WideToASCII(brand16));
  return ret;
}

#elif defined(OS_MACOSX)

bool GetBrand(std::string* brand) {
  brand->assign(keystone_glue::BrandCode());
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  brand->clear();
  return true;
}

#else

bool GetBrand(std::string* brand) {
  brand->clear();
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  brand->clear();
  return true;
}

#endif

bool IsOrganic(const std::string& brand) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kOrganicInstall))
    return true;

  static const char* kBrands[] = {
      "CHFO", "CHFT", "CHHS", "CHHM", "CHMA", "CHMB", "CHME", "CHMF",
      "CHMG", "CHMH", "CHMI", "CHMQ", "CHMV", "CHNB", "CHNC", "CHNG",
      "CHNH", "CHNI", "CHOA", "CHOB", "CHOC", "CHON", "CHOO", "CHOP",
      "CHOQ", "CHOR", "CHOS", "CHOT", "CHOU", "CHOX", "CHOY", "CHOZ",
      "CHPD", "CHPE", "CHPF", "CHPG", "EUBB", "EUBC", "GGLA", "GGLS"
  };
  const char** end = &kBrands[arraysize(kBrands)];
  const char** found = std::find(&kBrands[0], end, brand);
  if (found != end)
    return true;
  return (StartsWithASCII(brand, "EUB", true) ||
          StartsWithASCII(brand, "EUC", true) ||
          StartsWithASCII(brand, "GGR", true));
}

bool IsOrganicFirstRun(const std::string& brand) {
  // Used for testing, to force search engine selector to appear.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kOrganicInstall))
    return true;

  return (StartsWithASCII(brand, "GG", true) ||
          StartsWithASCII(brand, "EU", true));
}

}  // namespace google_util
