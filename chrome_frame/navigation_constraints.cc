// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/navigation_constraints.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "chrome_frame/utils.h"
#include "extensions/common/constants.h"

NavigationConstraintsImpl::NavigationConstraintsImpl() : is_privileged_(false) {
}

// NavigationConstraintsImpl method definitions.
bool NavigationConstraintsImpl::AllowUnsafeUrls() {
  // No sanity checks if unsafe URLs are allowed
  return GetConfigBool(false, kAllowUnsafeURLs);
}

bool NavigationConstraintsImpl::IsSchemeAllowed(const GURL& url) {
  if (url.is_empty())
    return false;

  if (!url.is_valid())
    return false;

  if (url.SchemeIs(chrome::kHttpScheme) ||
      url.SchemeIs(chrome::kHttpsScheme))
    return true;

  // Additional checking for view-source. Allow only http and https
  // URLs in view source.
  if (url.SchemeIs(chrome::kViewSourceScheme)) {
    GURL sub_url(url.path());
    if (sub_url.SchemeIs(chrome::kHttpScheme) ||
        sub_url.SchemeIs(chrome::kHttpsScheme))
      return true;
  }

  // Allow only about:blank or about:version
  if (url.SchemeIs(chrome::kAboutScheme)) {
    if (LowerCaseEqualsASCII(url.spec(), chrome::kAboutBlankURL) ||
        LowerCaseEqualsASCII(url.spec(), chrome::kAboutVersionURL)) {
      return true;
    }
  }

  if (is_privileged_ &&
      (url.SchemeIs(chrome::kDataScheme) ||
       url.SchemeIs(extensions::kExtensionScheme))) {
    return true;
  }

  return false;
}

bool NavigationConstraintsImpl::IsZoneAllowed(const GURL& url) {
  if (!security_manager_) {
    HRESULT hr = security_manager_.CreateInstance(
        CLSID_InternetSecurityManager);
    if (FAILED(hr)) {
      NOTREACHED() << __FUNCTION__
                   << " Failed to create SecurityManager. Error: 0x%x"
                   << hr;
      return true;
    }
    DWORD zone = URLZONE_INVALID;
    std::wstring unicode_url = UTF8ToWide(url.spec());
    security_manager_->MapUrlToZone(unicode_url.c_str(), &zone, 0);
    if (zone == URLZONE_UNTRUSTED) {
      DLOG(WARNING) << __FUNCTION__
                    << " Disallowing navigation to restricted url: " << url;
      return false;
    }
  }
  return true;
}

bool NavigationConstraintsImpl::is_privileged() const {
  return is_privileged_;
}

void NavigationConstraintsImpl::set_is_privileged(bool is_privileged) {
  is_privileged_ = is_privileged;
}
