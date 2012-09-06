// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_util.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mac/keystone_glue.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/google/linkdoctor_internal/linkdoctor_internal.h"
#endif

#ifndef LINKDOCTOR_SERVER_REQUEST_URL
#define LINKDOCTOR_SERVER_REQUEST_URL ""
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

bool gUseMockLinkDoctorBaseURLForTesting = false;

// Finds the first key-value pair where the key matches |query_key|. Returns
// true if a match is found and sets |search_terms| to the value.
bool ExtractSearchTermsFromComponent(const std::string& url,
                                     url_parse::Component* component,
                                     string16* search_terms) {
  const std::string query_key = "q";
  url_parse::Component key, value;

  while (url_parse::ExtractQueryKeyValue(url.c_str(), component,
                                         &key, &value)) {
    if (url.compare(key.begin, key.len, query_key) != 0)
      continue;
    std::string value_str = url.substr(value.begin, value.len);
    *search_terms = net::UnescapeAndDecodeUTF8URLComponent(
        value_str,
        net::UnescapeRule::SPACES |
            net::UnescapeRule::URL_SPECIAL_CHARS |
            net::UnescapeRule::REPLACE_PLUS_WITH_SPACE,
        NULL);
    return true;
  }
  return false;
}

}  // anonymous namespace

namespace google_util {

GURL LinkDoctorBaseURL() {
  if (gUseMockLinkDoctorBaseURLForTesting)
    return GURL("http://mock.linkdoctor.url/for?testing");
  return GURL(LINKDOCTOR_SERVER_REQUEST_URL);
}

void SetMockLinkDoctorBaseURLForTesting() {
  gUseMockLinkDoctorBaseURLForTesting = true;
}

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

string16 GetSearchTermsFromGoogleSearchURL(const std::string& url) {
  if (!IsInstantExtendedAPIGoogleSearchUrl(url))
    return string16();

  url_parse::Parsed parsed_url;
  url_parse::ParseStandardURL(url.c_str(), url.length(), &parsed_url);

  string16 search_terms;
  // The search terms can be in either the query or ref component - for
  // instance, in a regular Google search they'll be in the query but in a
  // Google Instant search they can be in both. The ref is the correct one to
  // return in this case, so test the ref component first.
  if (ExtractSearchTermsFromComponent(url, &parsed_url.ref, &search_terms) ||
      ExtractSearchTermsFromComponent(url, &parsed_url.query, &search_terms)) {
    return search_terms;
  }
  return string16();
}

bool IsGoogleDomainUrl(const std::string& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission) {
  GURL original_url(url);
  if (!original_url.is_valid() ||
      !(original_url.SchemeIs("http") || original_url.SchemeIs("https")))
    return false;

  // If we have the Instant URL overridden with a command line flag, accept
  // its domain/port combination as well.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kInstantURL)) {
    GURL custom_instant_url(
        command_line.GetSwitchValueASCII(switches::kInstantURL));
    if (original_url.host() == custom_instant_url.host() &&
        original_url.port() == custom_instant_url.port())
      return true;
  }

  return (original_url.port().empty() ||
      port_permission == ALLOW_NON_STANDARD_PORTS) &&
      google_util::IsGoogleHostname(original_url.host(), subdomain_permission);
}

bool IsGoogleHostname(const std::string& host,
                      SubdomainPermission subdomain_permission) {
  size_t tld_length =
      net::RegistryControlledDomainService::GetRegistryLength(host, false);
  if ((tld_length == 0) || (tld_length == std::string::npos))
    return false;
  std::string host_minus_tld(host, 0, host.length() - tld_length);
  if (LowerCaseEqualsASCII(host_minus_tld, "google."))
    return true;
  if (subdomain_permission == ALLOW_SUBDOMAIN)
    return EndsWith(host_minus_tld, ".google.", false);
  return LowerCaseEqualsASCII(host_minus_tld, "www.google.");
}

bool IsGoogleHomePageUrl(const std::string& url) {
  GURL original_url(url);

  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN, DISALLOW_NON_STANDARD_PORTS))
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
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN, DISALLOW_NON_STANDARD_PORTS))
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

bool IsInstantExtendedAPIGoogleSearchUrl(const std::string& url) {
  if (!IsGoogleSearchUrl(url))
    return false;

  const std::string embedded_search_key = kInstantExtendedAPIParam;

  url_parse::Parsed parsed_url;
  url_parse::ParseStandardURL(url.c_str(), url.length(), &parsed_url);
  url_parse::Component key, value;
  while (url_parse::ExtractQueryKeyValue(
      url.c_str(), &parsed_url.query, &key, &value)) {
    // If the parameter key is |embedded_search_key| and the value is not 0 this
    // is an Instant Extended API Google search URL.
    if (!url.compare(key.begin, key.len, embedded_search_key)) {
      int int_value = 0;
      if (value.is_nonempty())
        base::StringToInt(url.substr(value.begin, value.len), &int_value);
      return int_value != 0;
    }
  }
  return false;
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
