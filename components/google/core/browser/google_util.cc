// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_util.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/browser/google_switches.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/url_fixer/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

// Only use Link Doctor on official builds.  It uses an API key, too, but
// seems best to just disable it, for more responsive error pages and to reduce
// server load.
#if defined(GOOGLE_CHROME_BUILD)
#define LINKDOCTOR_SERVER_REQUEST_URL "https://www.googleapis.com/rpc"
#else
#define LINKDOCTOR_SERVER_REQUEST_URL ""
#endif


// Helpers --------------------------------------------------------------------

namespace {

bool gUseMockLinkDoctorBaseURLForTesting = false;

bool IsPathHomePageBase(const std::string& path) {
  return (path == "/") || (path == "/webhp");
}

// True if |host| is "[www.]<domain_in_lower_case>.<TLD>" with a valid TLD. If
// |subdomain_permission| is ALLOW_SUBDOMAIN, we check against host
// "*.<domain_in_lower_case>.<TLD>" instead.
bool IsValidHostName(const std::string& host,
                     const std::string& domain_in_lower_case,
                     google_util::SubdomainPermission subdomain_permission) {
  size_t tld_length = net::registry_controlled_domains::GetRegistryLength(
      host,
      net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if ((tld_length == 0) || (tld_length == std::string::npos))
    return false;
  // Removes the tld and the preceding dot.
  std::string host_minus_tld(host, 0, host.length() - tld_length - 1);
  if (LowerCaseEqualsASCII(host_minus_tld, domain_in_lower_case.c_str()))
    return true;
  if (subdomain_permission == google_util::ALLOW_SUBDOMAIN)
    return EndsWith(host_minus_tld, "." + domain_in_lower_case, false);
  return LowerCaseEqualsASCII(host_minus_tld,
                              ("www." + domain_in_lower_case).c_str());
}

// True if |url| is a valid URL with HTTP or HTTPS scheme. If |port_permission|
// is DISALLOW_NON_STANDARD_PORTS, this also requires |url| to use the standard
// port for its scheme (80 for HTTP, 443 for HTTPS).
bool IsValidURL(const GURL& url, google_util::PortPermission port_permission) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
      (url.port().empty() ||
       (port_permission == google_util::ALLOW_NON_STANDARD_PORTS));
}

}  // namespace


namespace google_util {

// Global functions -----------------------------------------------------------

bool HasGoogleSearchQueryParam(const std::string& str) {
  url::Component query(0, str.length()), key, value;
  while (url::ExtractQueryKeyValue(str.c_str(), &query, &key, &value)) {
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

std::string GetGoogleLocale(const std::string& application_locale) {
  // Google does not recognize "nb" for Norwegian Bokmal; it uses "no".
  return (application_locale == "nb") ? "no" : application_locale;
}

GURL AppendGoogleLocaleParam(const GURL& url,
                             const std::string& application_locale) {
  return net::AppendQueryParameter(
      url, "hl", GetGoogleLocale(application_locale));
}

std::string GetGoogleCountryCode(GURL google_homepage_url) {
  const std::string google_hostname = google_homepage_url.host();
  const size_t last_dot = google_hostname.find_last_of('.');
  if (last_dot == std::string::npos) {
    NOTREACHED();
  }
  std::string country_code = google_hostname.substr(last_dot + 1);
  // Assume the com TLD implies the US.
  if (country_code == "com")
    return "us";
  // Google uses the Unicode Common Locale Data Repository (CLDR), and the CLDR
  // code for the UK is "gb".
  if (country_code == "uk")
    return "gb";
  // Catalonia does not have a CLDR country code, since it's a region in Spain,
  // so use Spain instead.
  if (country_code == "cat")
    return "es";
  return country_code;
}

GURL GetGoogleSearchURL(GURL google_homepage_url) {
  // To transform the homepage URL into the corresponding search URL, add the
  // "search" and the "q=" query string.
  std::string search_path = "search";
  std::string query_string = "q=";
  GURL::Replacements replacements;
  replacements.SetPathStr(search_path);
  replacements.SetQueryStr(query_string);
  return google_homepage_url.ReplaceComponents(replacements);
}

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
    base_url = url_fixer::FixupURL(switch_value, std::string());
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

  return IsValidHostName(host, "google", subdomain_permission);
}

bool IsGoogleDomainUrl(const GURL& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission) {
  return IsValidURL(url, port_permission) &&
      IsGoogleHostname(url.host(), subdomain_permission);
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

bool IsYoutubeDomainUrl(const GURL& url,
                        SubdomainPermission subdomain_permission,
                        PortPermission port_permission) {
  return IsValidURL(url, port_permission) &&
      IsValidHostName(url.host(), "youtube", subdomain_permission);
}

}  // namespace google_util
