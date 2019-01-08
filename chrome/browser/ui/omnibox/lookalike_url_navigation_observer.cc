// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/lookalike_url_navigation_observer.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/common/chrome_features.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/idn_spoof_checker.h"
#include "components/url_formatter/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

#include "components/url_formatter/top_domains/top500-domains-inc.cc"

using MatchType = LookalikeUrlNavigationObserver::MatchType;
using NavigationSuggestionEvent =
    LookalikeUrlNavigationObserver::NavigationSuggestionEvent;

void RecordEvent(
    LookalikeUrlNavigationObserver::NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(LookalikeUrlNavigationObserver::kHistogramName,
                            event);
}

bool SkeletonsMatch(const url_formatter::Skeletons& skeletons1,
                    const url_formatter::Skeletons& skeletons2) {
  DCHECK(!skeletons1.empty());
  DCHECK(!skeletons2.empty());
  for (const std::string& skeleton1 : skeletons1) {
    if (base::ContainsKey(skeletons2, skeleton1))
      return true;
  }
  return false;
}

// Returns the eTLD+1 of |hostname|. This excludes private registries so that
// GetETLDPlusOne("test.blogspot.com") returns "blogspot.com" (blogspot.com is
// listed as a private registry). We do this to be consistent with
// url_formatter's top domain list which doesn't have a notion of private
// registries.
std::string GetETLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

// Returns a site that the user has used before that the eTLD+1 in
// |domain_and_registry| may be attempting to spoof, based on skeleton
// comparison.
std::string GetMatchingSiteEngagementDomain(
    SiteEngagementService* service,
    const std::string& domain_and_registry) {
  // Compute skeletons using eTLD+1.
  // eTLD+1 can be empty for private domains.
  if (domain_and_registry.empty())
    return std::string();

  url_formatter::IDNConversionResult result =
      url_formatter::IDNToUnicodeWithDetails(domain_and_registry);
  DCHECK(result.has_idn_component);
  const url_formatter::Skeletons navigated_skeletons =
      url_formatter::GetSkeletons(result.result);

  std::map<std::string, url_formatter::Skeletons>
      domain_and_registry_to_skeleton;
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      service->GetAllDetails();
  for (const auto& detail : engagement_details) {
    // Ignore sites with an engagement score lower than LOW.
    if (!service->IsEngagementAtLeast(detail.origin,
                                      blink::mojom::EngagementLevel::MEDIUM))
      continue;

    // Site engagement service should only return HTTP or HTTPS domains.
    DCHECK(detail.origin.SchemeIsHTTPOrHTTPS());

    // If the user has engaged with eTLD+1 of this site, don't show any
    // lookalike navigation suggestions.
    const std::string engaged_domain_and_registry =
        GetETLDPlusOne(detail.origin);
    // eTLD+1 can be empty for private domains (e.g. http://test).
    if (engaged_domain_and_registry.empty())
      continue;

    if (domain_and_registry == engaged_domain_and_registry)
      return std::string();

    // Multiple domains can map to the same eTLD+1, avoid skeleton generation
    // when possible.
    auto it = domain_and_registry_to_skeleton.find(engaged_domain_and_registry);
    url_formatter::Skeletons skeletons;
    if (it == domain_and_registry_to_skeleton.end()) {
      // Engaged site can be IDN. Decode as unicode and compute the skeleton
      // from that. At this point, top domain checks have already been done, so
      // if the site is IDN, it'll always be decoded as unicode (i.e. IDN spoof
      // checker will not find a matching top domain and fall back to punycode
      // for it).
      url_formatter::IDNConversionResult conversion_result =
          url_formatter::IDNToUnicodeWithDetails(engaged_domain_and_registry);

      skeletons = url_formatter::GetSkeletons(conversion_result.result);
      domain_and_registry_to_skeleton[engaged_domain_and_registry] = skeletons;
    } else {
      skeletons = it->second;
    }

    if (SkeletonsMatch(navigated_skeletons, skeletons))
      return detail.origin.host();
  }
  return std::string();
}

}  // namespace

// static
const char LookalikeUrlNavigationObserver::kHistogramName[] =
    "NavigationSuggestion.Event";

LookalikeUrlNavigationObserver::LookalikeUrlNavigationObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

LookalikeUrlNavigationObserver::~LookalikeUrlNavigationObserver() {}

void LookalikeUrlNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore subframe and same document navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  // If the navigation was not committed, it means either the page was a
  // download or error 204/205, or the navigation never left the previous
  // URL. Basically, this isn't a problem since we stayed at the existing URL.
  if (!navigation_handle->HasCommitted())
    return;

  const GURL url = navigation_handle->GetURL();
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  // If the user has engaged with this site, don't show any lookalike
  // navigation suggestions.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  if (service->IsEngagementAtLeast(url, blink::mojom::EngagementLevel::MEDIUM))
    return;

  std::string matched_domain;
  MatchType match_type;
  if (!GetMatchingDomain(url, service, &matched_domain, &match_type))
    return;

  DCHECK(!matched_domain.empty());

  GURL::Replacements replace_host;
  replace_host.SetHostStr(matched_domain);
  const GURL suggested_url = url.ReplaceComponents(replace_host);

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  CHECK(ukm_recorder);
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents());
  ukm::builders::LookalikeUrl_NavigationSuggestion(source_id)
      .SetMatchType(static_cast<int>(match_type))
      .Record(ukm_recorder);

  if (base::FeatureList::IsEnabled(
          features::kLookalikeUrlNavigationSuggestionsUI)) {
    RecordEvent(NavigationSuggestionEvent::kInfobarShown);
    AlternateNavInfoBarDelegate::CreateForLookalikeUrlNavigation(
        web_contents(), base::UTF8ToUTF16(matched_domain), suggested_url, url,
        base::BindOnce(RecordEvent, NavigationSuggestionEvent::kLinkClicked));
  }
}

bool LookalikeUrlNavigationObserver::GetMatchingDomain(
    const GURL& url,
    SiteEngagementService* service,
    std::string* matched_domain,
    MatchType* match_type) {
  // Perform all computations on eTLD+1.
  const std::string domain_and_registry = GetETLDPlusOne(url);

  url_formatter::IDNConversionResult result =
      url_formatter::IDNToUnicodeWithDetails(domain_and_registry);
  if (result.has_idn_component) {
    // If the navigated domain is IDN, check its skeleton against top domains
    // and engaged sites.
    if (!result.matching_top_domain.empty()) {
      // In practice, this is not possible since the top domain list does not
      // contain IDNs, so domain_and_registry can't both have IDN and be a top
      // domain. Still, sanity check in case the top domain list changes in the
      // future.
      if (domain_and_registry == result.matching_top_domain) {
        // Navigated domain is a top domain, do nothing.
        return false;
      }
      RecordEvent(NavigationSuggestionEvent::kMatchTopSite);
      *matched_domain = result.matching_top_domain;
      *match_type = MatchType::kTopSite;
      return true;
    }

    const std::string matched_engaged_domain =
        GetMatchingSiteEngagementDomain(service, domain_and_registry);
    if (!matched_engaged_domain.empty()) {
      RecordEvent(NavigationSuggestionEvent::kMatchSiteEngagement);
      *matched_domain = matched_engaged_domain;
      *match_type = MatchType::kSiteEngagement;
      return true;
    }
  }

  // If we can't find an exact top domain or an engaged site, try to find a top
  // domain within an edit distance of one.
  const std::string similar_domain =
      GetSimilarDomainFromTop500(base::UTF16ToUTF8(result.result));
  if (!similar_domain.empty() && domain_and_registry != similar_domain) {
    RecordEvent(NavigationSuggestionEvent::kMatchEditDistance);
    *matched_domain = similar_domain;
    *match_type = MatchType::kEditDistance;
    return true;
  }
  return false;
}

// static
bool LookalikeUrlNavigationObserver::IsEditDistanceAtMostOne(
    const base::string16& str1,
    const base::string16& str2) {
  if (str1.size() > str2.size() + 1 || str2.size() > str1.size() + 1) {
    return false;
  }
  base::string16::const_iterator i = str1.begin();
  base::string16::const_iterator j = str2.begin();
  size_t edit_count = 0;
  while (i != str1.end() && j != str2.end()) {
    if (*i == *j) {
      i++;
      j++;
    } else {
      edit_count++;
      if (edit_count > 1) {
        return false;
      }

      if (str1.size() > str2.size()) {
        // First string is longer than the second. This can only happen if the
        // first string has an extra character.
        i++;
      } else if (str2.size() > str1.size()) {
        // Second string is longer than the first. This can only happen if the
        // second string has an extra character.
        j++;
      } else {
        // Both strings are the same length. This can only happen if the two
        // strings differ by a single character.
        i++;
        j++;
      }
    }
  }
  if (i != str1.end() || j != str2.end()) {
    // A character at the end did not match.
    edit_count++;
  }
  return edit_count <= 1;
}

// static
std::string LookalikeUrlNavigationObserver::GetSimilarDomainFromTop500(
    const std::string& domain_and_registry) {
  if (!url_formatter::top_domains::IsEditDistanceCandidate(
          domain_and_registry)) {
    return std::string();
  }
  const std::string domain_without_registry =
      url_formatter::top_domains::HostnameWithoutRegistry(domain_and_registry);

  for (const std::string& skeleton :
       url_formatter::GetSkeletons(base::UTF8ToUTF16(domain_and_registry))) {
    for (const char* const top_domain_skeleton : kTop500) {
      if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(skeleton),
                                  base::UTF8ToUTF16(top_domain_skeleton))) {
        const std::string top_domain =
            url_formatter::LookupSkeletonInTopDomains(top_domain_skeleton);
        DCHECK(!top_domain.empty());
        // If the only difference between the navigated and top
        // domains is the registry part, this is unlikely to be a spoofing
        // attempt. Ignore this match and continue. E.g. If the navigated domain
        // is google.com.tw and the top domain is google.com.tr, this won't
        // produce a match.
        const std::string top_domain_without_registry =
            url_formatter::top_domains::HostnameWithoutRegistry(top_domain);
        if (domain_without_registry != top_domain_without_registry) {
          return top_domain;
        }
      }
    }
  }
  return std::string();
}

// static
void LookalikeUrlNavigationObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<LookalikeUrlNavigationObserver>(web_contents));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LookalikeUrlNavigationObserver)
