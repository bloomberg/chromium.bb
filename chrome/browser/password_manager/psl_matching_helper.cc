// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/psl_matching_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/core/common/password_form.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using autofill::PasswordForm;

bool PSLMatchingHelper::psl_enabled_override_ = false;

PSLMatchingHelper::PSLMatchingHelper() : psl_enabled_(DeterminePSLEnabled()) {}

PSLMatchingHelper::~PSLMatchingHelper() {}

bool PSLMatchingHelper::IsMatchingEnabled() const {
  return psl_enabled_override_ || psl_enabled_;
}

bool PSLMatchingHelper::ShouldPSLDomainMatchingApply(
    const std::string& registry_controlled_domain) const {
  return IsMatchingEnabled() && registry_controlled_domain != "google.com";
}

// static
bool PSLMatchingHelper::IsPublicSuffixDomainMatch(const std::string& url1,
                                                  const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);
  return gurl1.scheme() == gurl2.scheme() &&
         GetRegistryControlledDomain(gurl1) ==
             GetRegistryControlledDomain(gurl2) &&
         gurl1.port() == gurl2.port();
}

// static
std::string PSLMatchingHelper::GetRegistryControlledDomain(
    const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// static
void PSLMatchingHelper::EnablePublicSuffixDomainMatchingForTesting() {
  psl_enabled_override_ = true;
}

// static
bool PSLMatchingHelper::DeterminePSLEnabled() {
#if defined(OS_ANDROID)
  // Default choice is "enabled", so we do not need to check for
  // kEnablePasswordAutofillPublicSuffixDomainMatching.
  bool enabled = true;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePasswordAutofillPublicSuffixDomainMatching)) {
    enabled = false;
  }
  return enabled;
#else
  return false;
#endif
}
