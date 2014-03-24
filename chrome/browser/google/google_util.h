// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some Google related utility functions.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_H__
#define CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_H__

#include <string>

#include "base/basictypes.h"

class GURL;
class Profile;

// This namespace provides various helpers around handling Google-related URLs
// and state relating to Google Chrome distributions (such as RLZ).
namespace google_util {

// True iff |str| contains a "q=" query parameter with a non-empty value.
// |str| should be a query or a hash fragment, without the ? or # (as
// returned by GURL::query() or GURL::ref().
bool HasGoogleSearchQueryParam(const std::string& str);

// The query key that identifies a Google Extended API request for Instant.
const char kInstantExtendedAPIParam[] = "espv";

GURL LinkDoctorBaseURL();
void SetMockLinkDoctorBaseURLForTesting();

// Returns the Google locale.  This is the same string as
// AppendGoogleLocaleParam adds to the URL, only without the leading "hl".
std::string GetGoogleLocale();

// Adds the Google locale string to the URL (e.g., hl=en-US).  This does not
// check to see if the param already exists.
GURL AppendGoogleLocaleParam(const GURL& url);

// String version of AppendGoogleLocaleParam.
std::string StringAppendGoogleLocaleParam(const std::string& url);

// Returns the Google country code string for the given profile.
std::string GetGoogleCountryCode(Profile* profile);

// Returns the Google search URL for the given profile.
GURL GetGoogleSearchURL(Profile* profile);

// Returns in |brand| the brand code or distribution tag that has been
// assigned to a partner. Returns false if the information is not available.
bool GetBrand(std::string* brand);

// Returns in |brand| the reactivation brand code or distribution tag
// that has been assigned to a partner for reactivating a dormant chrome
// install. Returns false if the information is not available.
bool GetReactivationBrand(std::string* brand);

// Returns the Google base URL specified on the command line, if it exists.
// This performs some fixup and sanity-checking to ensure that the resulting URL
// is valid and has no query or ref.
GURL CommandLineGoogleBaseURL();

// Returns true if a Google base URL was specified on the command line and |url|
// begins with that base URL.  This uses a simple string equality check.
bool StartsWithCommandLineGoogleBaseURL(const GURL& url);

// WARNING: The following IsGoogleXXX() functions use heuristics to rule out
// "obviously false" answers.  They do NOT guarantee that the string in question
// is actually on a Google-owned domain, just that it looks plausible.  If you
// need to restrict some behavior to only happen on Google's officially-owned
// domains, use TransportSecurityState::IsGooglePinnedProperty() instead.

// Designate whether or not a URL checking function also checks for specific
// subdomains, or only "www" and empty subdomains.
enum SubdomainPermission {
  ALLOW_SUBDOMAIN,
  DISALLOW_SUBDOMAIN,
};

// Designate whether or not a URL checking function also checks for standard
// ports (80 for http, 443 for https), or if it allows all port numbers.
enum PortPermission {
  ALLOW_NON_STANDARD_PORTS,
  DISALLOW_NON_STANDARD_PORTS,
};

// True if |host| is "[www.]google.<TLD>" with a valid TLD. If
// |subdomain_permission| is ALLOW_SUBDOMAIN, we check against host
// "*.google.<TLD>" instead.
//
// If the Google base URL has been overridden on the command line, this function
// will also return true for any URL whose hostname exactly matches the hostname
// of the URL specified on the command line.  In this case,
// |subdomain_permission| is ignored.
bool IsGoogleHostname(const std::string& host,
                      SubdomainPermission subdomain_permission);

// True if |url| is a valid URL with a host that returns true for
// IsGoogleHostname(), and an HTTP or HTTPS scheme.  If |port_permission| is
// DISALLOW_NON_STANDARD_PORTS, this also requires |url| to use the standard
// port for its scheme (80 for HTTP, 443 for HTTPS).
//
// Note that this only checks for google.<TLD> domains, but not other Google
// properties. There is code in variations_http_header_provider.cc that checks
// for additional Google properties, which can be moved here if more callers
// are interested in this in the future.
bool IsGoogleDomainUrl(const GURL& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission);

// True if |url| represents a valid Google home page URL.
bool IsGoogleHomePageUrl(const GURL& url);

// True if |url| represents a valid Google search URL.
bool IsGoogleSearchUrl(const GURL& url);

// True if a build is strictly organic, according to its brand code.
bool IsOrganic(const std::string& brand);

// True if a build should run as organic during first run. This uses
// a slightly different set of brand codes from the standard IsOrganic
// method.
bool IsOrganicFirstRun(const std::string& brand);

// True if |brand| is an internet cafe brand code.
bool IsInternetCafeBrandCode(const std::string& brand);

// This class is meant to be used only from test code, and sets the brand
// code returned by the function GetBrand() above while the object exists.
class BrandForTesting {
 public:
  explicit BrandForTesting(const std::string& brand);
  ~BrandForTesting();

 private:
  std::string brand_;
  DISALLOW_COPY_AND_ASSIGN(BrandForTesting);
};

}  // namespace google_util

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_H__
