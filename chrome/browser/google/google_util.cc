// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_util.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/string16.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mac/keystone_glue.h"
#endif

namespace {

const char* brand_for_testing = NULL;

// True iff |str| contains a "q=" query parameter with a non-empty value.
// |str| should be a URL parameter or a hash fragment, without the ? or # (as
// returned by GURL::query() or GURL::ref().
bool HasQueryParameter(const std::string& str) {
  std::vector<std::string> parameters;

  base::SplitString(str, '&', &parameters);
  for (std::vector<std::string>::const_iterator itr = parameters.begin();
       itr != parameters.end();
       ++itr) {
    if (StartsWithASCII(*itr, "q=", false) && itr->size() > 2)
      return true;
  }
  return false;
}

}  // anonymous namespace

namespace google_util {

const char kLinkDoctorBaseURL[] =
    "http://linkhelp.clients.google.com/tbproxy/lh/fixurl";

BrandForTesting::BrandForTesting(const std::string& brand) : brand_(brand) {
  DCHECK(brand_for_testing == NULL);
  brand_for_testing = brand_.c_str();
}

BrandForTesting::~BrandForTesting() {
  brand_for_testing = NULL;
}

GURL AppendGoogleLocaleParam(const GURL& url) {
  // Google does not yet recognize 'nb' for Norwegian Bokmal, but it uses
  // 'no' for that.
  std::string locale = g_browser_process->GetApplicationLocale();
  if (locale == "nb")
    locale = "no";
  return chrome_common_net::AppendQueryParameter(url, "hl", locale);
}

std::string StringAppendGoogleLocaleParam(const std::string& url) {
  GURL original_url(url);
  DCHECK(original_url.is_valid());
  GURL localized_url = AppendGoogleLocaleParam(original_url);
  return localized_url.spec();
}

GURL AppendGoogleTLDParam(Profile* profile, const GURL& url) {
  const std::string google_domain(
      net::RegistryControlledDomainService::GetDomainAndRegistry(
          GoogleURLTracker::GoogleURL(profile)));
  const size_t first_dot = google_domain.find('.');
  if (first_dot == std::string::npos) {
    NOTREACHED();
    return url;
  }
  return chrome_common_net::AppendQueryParameter(
      url, "sd", google_domain.substr(first_dot + 1));
}

#if defined(OS_WIN)

bool GetBrand(std::string* brand) {
  if (brand_for_testing) {
    brand->assign(brand_for_testing);
    return true;
  }

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

#else

bool GetBrand(std::string* brand) {
  if (brand_for_testing) {
    brand->assign(brand_for_testing);
    return true;
  }

#if defined(OS_MACOSX)
  brand->assign(keystone_glue::BrandCode());
#else
  brand->clear();
#endif
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  brand->clear();
  return true;
}

#endif

bool IsGoogleDomainUrl(const std::string& url, SubdomainPermission permission) {
  GURL original_url(url);
  return original_url.is_valid() && original_url.port().empty() &&
      (original_url.SchemeIs("http") || original_url.SchemeIs("https")) &&
      google_util::IsGoogleHostname(original_url.host(), permission);
}

bool IsGoogleHostname(const std::string& host,
                      SubdomainPermission permission) {
  size_t tld_length =
      net::RegistryControlledDomainService::GetRegistryLength(host, false);
  if ((tld_length == 0) || (tld_length == std::string::npos))
    return false;
  std::string host_minus_tld(host, 0, host.length() - tld_length);
  if (LowerCaseEqualsASCII(host_minus_tld, "google."))
    return true;
  if (permission == ALLOW_SUBDOMAIN)
    return EndsWith(host_minus_tld, ".google.", false);
  return LowerCaseEqualsASCII(host_minus_tld, "www.google.");
}

bool IsGoogleHomePageUrl(const std::string& url) {
  GURL original_url(url);

  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN))
    return false;

  // Make sure the path is a known home page path.
  std::string path(original_url.path());
  if (path != "/" && path != "/webhp" &&
      !StartsWithASCII(path, "/ig", false)) {
    return false;
  }

  return true;
}

bool IsGoogleSearchUrl(const std::string& url) {
  GURL original_url(url);

  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN))
    return false;

  // Make sure the path is a known search path.
  std::string path(original_url.path());
  bool has_valid_path = false;
  bool is_home_page_base = false;
  if (path == "/search") {
    has_valid_path = true;
  } else if (path == "/webhp" || path == "/") {
    // Note that we allow both "/" and "" paths, but GURL spits them
    // both out as just "/".
    has_valid_path = true;
    is_home_page_base = true;
  }
  if (!has_valid_path)
    return false;

  // Check for query parameter in URL parameter and hash fragment, depending on
  // the path type.
  std::string query(original_url.query());
  std::string ref(original_url.ref());
  return HasQueryParameter(ref) ||
      (!is_home_page_base && HasQueryParameter(query));
}

bool IsOrganic(const std::string& brand) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kOrganicInstall))
    return true;

#if defined(OS_MACOSX)
  if (brand.empty()) {
    // An empty brand string on Mac is used for channels other than stable,
    // which are always organic.
    return true;
  }
#endif

  const char* const kBrands[] = {
      "CHCA", "CHCB", "CHCG", "CHCH", "CHCI", "CHCJ", "CHCK", "CHCL",
      "CHFO", "CHFT", "CHHS", "CHHM", "CHMA", "CHMB", "CHME", "CHMF",
      "CHMG", "CHMH", "CHMI", "CHMQ", "CHMV", "CHNB", "CHNC", "CHNG",
      "CHNH", "CHNI", "CHOA", "CHOB", "CHOC", "CHON", "CHOO", "CHOP",
      "CHOQ", "CHOR", "CHOS", "CHOT", "CHOU", "CHOX", "CHOY", "CHOZ",
      "CHPD", "CHPE", "CHPF", "CHPG", "ECBA", "ECBB", "ECDA", "ECDB",
      "ECSA", "ECSB", "ECVA", "ECVB", "ECWA", "ECWB", "ECWC", "ECWD",
      "ECWE", "ECWF", "EUBB", "EUBC", "GGLA", "GGLS"
  };
  const char* const* end = &kBrands[arraysize(kBrands)];
  const char* const* found = std::find(&kBrands[0], end, brand);
  if (found != end)
    return true;

  return StartsWithASCII(brand, "EUB", true) ||
         StartsWithASCII(brand, "EUC", true) ||
         StartsWithASCII(brand, "GGR", true);
}

bool IsOrganicFirstRun(const std::string& brand) {
  // Used for testing, to force search engine selector to appear.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kOrganicInstall))
    return true;

#if defined(OS_MACOSX)
  if (brand.empty()) {
    // An empty brand string on Mac is used for channels other than stable,
    // which are always organic.
    return true;
  }
#endif

  return StartsWithASCII(brand, "GG", true) ||
         StartsWithASCII(brand, "EU", true);
}

bool IsInternetCafeBrandCode(const std::string& brand) {
  const char* const kBrands[] = {
    "CHIQ", "CHSG", "HLJY", "NTMO", "OOBA", "OOBB", "OOBC", "OOBD", "OOBE",
    "OOBF", "OOBG", "OOBH", "OOBI", "OOBJ", "IDCM",
  };
  const char* const* end = &kBrands[arraysize(kBrands)];
  const char* const* found = std::find(&kBrands[0], end, brand);
  return found != end;
}

}  // namespace google_util
