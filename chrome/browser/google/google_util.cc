// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_util.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "chrome/installer/util/google_update_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mac/keystone_glue.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/google/google_util_chromeos.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/google/linkdoctor_internal/linkdoctor_internal.h"
#endif

#ifndef LINKDOCTOR_SERVER_REQUEST_URL
#define LINKDOCTOR_SERVER_REQUEST_URL std::string()
#endif


// Helpers --------------------------------------------------------------------

namespace {

const char* brand_for_testing = NULL;
bool gUseMockLinkDoctorBaseURLForTesting = false;

bool IsPathHomePageBase(const std::string& path) {
  return (path == "/") || (path == "/webhp");
}

}  // namespace


namespace google_util {

// Global functions -----------------------------------------------------------

bool HasGoogleSearchQueryParam(const std::string& str) {
  url_parse::Component query(0, str.length()), key, value;
  while (url_parse::ExtractQueryKeyValue(str.c_str(), &query, &key,
                                         &value)) {
    if ((key.len == 1) && (str[key.begin] == 'q') && value.is_nonempty())
      return true;
  }
  return false;
}

GURL LinkDoctorBaseURL() {
  if (gUseMockLinkDoctorBaseURLForTesting)
    return GURL("http://mock.linkdoctor.url/for?testing");
  return GURL(LINKDOCTOR_SERVER_REQUEST_URL);
}

void SetMockLinkDoctorBaseURLForTesting() {
  gUseMockLinkDoctorBaseURLForTesting = true;
}

GURL AppendGoogleLocaleParam(const GURL& url) {
  // Google does not yet recognize 'nb' for Norwegian Bokmal, but it uses
  // 'no' for that.
  std::string locale = g_browser_process->GetApplicationLocale();
  if (locale == "nb")
    locale = "no";
  return net::AppendQueryParameter(url, "hl", locale);
}

std::string StringAppendGoogleLocaleParam(const std::string& url) {
  GURL original_url(url);
  DCHECK(original_url.is_valid());
  GURL localized_url = AppendGoogleLocaleParam(original_url);
  return localized_url.spec();
}

GURL AppendGoogleTLDParam(Profile* profile, const GURL& url) {
  const std::string google_domain(
      net::registry_controlled_domains::GetDomainAndRegistry(
          GoogleURLTracker::GoogleURL(profile),
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES));
  const size_t first_dot = google_domain.find('.');
  if (first_dot == std::string::npos) {
    NOTREACHED();
    return url;
  }
  return net::AppendQueryParameter(url, "sd",
                                   google_domain.substr(first_dot + 1));
}

#if defined(OS_WIN)

bool GetBrand(std::string* brand) {
  if (brand_for_testing) {
    brand->assign(brand_for_testing);
    return true;
  }

  base::string16 brand16;
  bool ret = GoogleUpdateSettings::GetBrand(&brand16);
  if (ret)
    brand->assign(base::UTF16ToASCII(brand16));
  return ret;
}

bool GetReactivationBrand(std::string* brand) {
  base::string16 brand16;
  bool ret = GoogleUpdateSettings::GetReactivationBrand(&brand16);
  if (ret)
    brand->assign(base::UTF16ToASCII(brand16));
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
#elif defined(OS_CHROMEOS)
  brand->assign(google_util::chromeos::GetBrand());
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

GURL CommandLineGoogleBaseURL() {
  // Unit tests may add command-line flags after the first call to this
  // function, so we don't simply initialize a static |base_url| directly and
  // then unconditionally return it.
  CR_DEFINE_STATIC_LOCAL(std::string, switch_value, ());
  CR_DEFINE_STATIC_LOCAL(GURL, base_url, ());
  std::string current_switch_value(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kGoogleBaseURL));
  if (current_switch_value != switch_value) {
    switch_value = current_switch_value;
    base_url = URLFixerUpper::FixupURL(switch_value, std::string());
    if (!base_url.is_valid() || base_url.has_query() || base_url.has_ref())
      base_url = GURL();
  }
  return base_url;
}

bool StartsWithCommandLineGoogleBaseURL(const GURL& url) {
  GURL base_url(CommandLineGoogleBaseURL());
  return base_url.is_valid() &&
      StartsWithASCII(url.possibly_invalid_spec(), base_url.spec(), true);
}

bool IsGoogleHostname(const std::string& host,
                      SubdomainPermission subdomain_permission) {
  GURL base_url(CommandLineGoogleBaseURL());
  if (base_url.is_valid() && (host == base_url.host()))
    return true;

  size_t tld_length = net::registry_controlled_domains::GetRegistryLength(
      host,
      net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if ((tld_length == 0) || (tld_length == std::string::npos))
    return false;
  std::string host_minus_tld(host, 0, host.length() - tld_length);
  if (LowerCaseEqualsASCII(host_minus_tld, "google."))
    return true;
  if (subdomain_permission == ALLOW_SUBDOMAIN)
    return EndsWith(host_minus_tld, ".google.", false);
  return LowerCaseEqualsASCII(host_minus_tld, "www.google.");
}

bool IsGoogleDomainUrl(const GURL& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
      (url.port().empty() || (port_permission == ALLOW_NON_STANDARD_PORTS)) &&
      google_util::IsGoogleHostname(url.host(), subdomain_permission);
}

bool IsGoogleHomePageUrl(const GURL& url) {
  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN, DISALLOW_NON_STANDARD_PORTS))
    return false;

  // Make sure the path is a known home page path.
  std::string path(url.path());
  return IsPathHomePageBase(path) || StartsWithASCII(path, "/ig", false);
}

bool IsGoogleSearchUrl(const GURL& url) {
  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN, DISALLOW_NON_STANDARD_PORTS))
    return false;

  // Make sure the path is a known search path.
  std::string path(url.path());
  bool is_home_page_base = IsPathHomePageBase(path);
  if (!is_home_page_base && (path != "/search"))
    return false;

  // Check for query parameter in URL parameter and hash fragment, depending on
  // the path type.
  return HasGoogleSearchQueryParam(url.ref()) ||
      (!is_home_page_base && HasGoogleSearchQueryParam(url.query()));
}

bool IsOrganic(const std::string& brand) {
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


// BrandForTesting ------------------------------------------------------------

BrandForTesting::BrandForTesting(const std::string& brand) : brand_(brand) {
  DCHECK(brand_for_testing == NULL);
  brand_for_testing = brand_.c_str();
}

BrandForTesting::~BrandForTesting() {
  brand_for_testing = NULL;
}


}  // namespace google_util
