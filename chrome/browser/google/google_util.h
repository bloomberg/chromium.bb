// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some Google related utility functions.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_H__
#define CHROME_BROWSER_GOOGLE_GOOGLE_UTIL_H__
#pragma once

#include <string>

#include <base/basictypes.h>

class GURL;

namespace google_util {

extern const char kLinkDoctorBaseURL[];

// Adds the Google locale string to the URL (e.g., hl=en-US).  This does not
// check to see if the param already exists.
GURL AppendGoogleLocaleParam(const GURL& url);

// String version of AppendGoogleLocaleParam.
std::string StringAppendGoogleLocaleParam(const std::string& url);

// Adds the Google TLD string to the URL (e.g., sd=com).  This does not
// check to see if the param already exists.
GURL AppendGoogleTLDParam(const GURL& url);

// Returns in |brand| the brand code or distribution tag that has been
// assigned to a partner. Returns false if the information is not available.
bool GetBrand(std::string* brand);

// Returns in |brand| the reactivation brand code or distribution tag
// that has been assigned to a partner for reactivating a dormant chrome
// install. Returns false if the information is not available.
bool GetReactivationBrand(std::string* brand);

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
