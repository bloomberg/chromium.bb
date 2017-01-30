// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/psl_matching_helper.h"

#include <memory>
#include <ostream>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/password_form.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using autofill::PasswordForm;

namespace password_manager {

std::ostream& operator<<(std::ostream& out, MatchResult result) {
  switch (result) {
    case MatchResult::NO_MATCH:
      return out << "No Match";
    case MatchResult::EXACT_MATCH:
      return out << "Exact Match";
    case MatchResult::PSL_MATCH:
      return out << "PSL Match";
    case MatchResult::FEDERATED_MATCH:
      return out << "Federated Match";
    case MatchResult::FEDERATED_PSL_MATCH:
      return out << "Federated PSL Match";
  }
  // This should never be reached, it is simply here to suppress compiler
  // warnings.
  return out;
}

MatchResult GetMatchResult(const PasswordForm& form,
                           const PasswordStore::FormDigest& form_digest) {
  if (form.signon_realm == form_digest.signon_realm)
    return MatchResult::EXACT_MATCH;

  // PSL and federated matches only apply to HTML forms.
  if (form_digest.scheme != PasswordForm::SCHEME_HTML ||
      form.scheme != PasswordForm::SCHEME_HTML)
    return MatchResult::NO_MATCH;

  const bool allow_psl_match = ShouldPSLDomainMatchingApply(
      GetRegistryControlledDomain(GURL(form_digest.signon_realm)));
  const bool allow_federated_match = !form.federation_origin.unique();

  if (allow_psl_match &&
      IsPublicSuffixDomainMatch(form.signon_realm, form_digest.signon_realm))
    return MatchResult::PSL_MATCH;

  if (allow_federated_match &&
      IsFederatedMatch(form.signon_realm, form_digest.origin))
    return MatchResult::FEDERATED_MATCH;

  if (allow_psl_match && allow_federated_match &&
      IsFederatedPSLMatch(form.signon_realm, form_digest.origin))
    return MatchResult::FEDERATED_PSL_MATCH;

  return MatchResult::NO_MATCH;
}

bool ShouldPSLDomainMatchingApply(
    const std::string& registry_controlled_domain) {
  return !registry_controlled_domain.empty() &&
         registry_controlled_domain != "google.com";
}

bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);

  if (!gurl1.is_valid() || !gurl2.is_valid())
    return false;

  if (gurl1 == gurl2)
    return true;

  std::string domain1(GetRegistryControlledDomain(gurl1));
  std::string domain2(GetRegistryControlledDomain(gurl2));

  if (domain1.empty() || domain2.empty())
    return false;

  return gurl1.scheme() == gurl2.scheme() && domain1 == domain2 &&
         gurl1.port() == gurl2.port();
}

std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsFederatedMatch(const std::string& signon_realm, const GURL& origin) {
  // Federated matches only apply to HTTPS.
  if (!origin.SchemeIs(url::kHttpsScheme))
    return false;

  // The format should be "federation://origin.host/federation.host;
  std::string federated_realm = "federation://" + origin.host() + "/";
  return signon_realm.size() > federated_realm.size() &&
         base::StartsWith(signon_realm, federated_realm,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsFederatedPSLMatch(const std::string& signon_realm, const GURL& origin) {
  // The format should be "federation://origin.host/federation.host;
  // Check for presence of "federation://" prefix.
  static constexpr char federation_prefix[] = "federation://";
  if (!base::StartsWith(signon_realm, federation_prefix,
                        base::CompareCase::INSENSITIVE_ASCII))
    return false;

  // Replace federation scheme with HTTPS. This results in correct parsing of
  // host and path, and forces origin to have a HTTPS scheme in order to return
  // true.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  GURL https_signon_realm = GURL(signon_realm).ReplaceComponents(replacements);

  // Check for non-empty federation.host.
  if (!https_signon_realm.has_path() || https_signon_realm.path_piece() == "/")
    return false;

  return IsPublicSuffixDomainMatch(https_signon_realm.GetOrigin().spec(),
                                   origin.GetOrigin().spec());
}
}  // namespace password_manager
