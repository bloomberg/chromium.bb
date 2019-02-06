// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_OBSERVER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

class Profile;

// Observes navigations and shows an infobar if the navigated domain name
// is visually similar to a top domain or a domain with a site engagement score.
class LookalikeUrlNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LookalikeUrlNavigationObserver> {
 public:
  // Used for metrics. Multiple events can occur per navigation.
  enum class NavigationSuggestionEvent {
    kNone = 0,
    kInfobarShown = 1,
    kLinkClicked = 2,
    kMatchTopSite = 3,
    kMatchSiteEngagement = 4,
    kMatchEditDistance = 5,

    // Append new items to the end of the list above; do not modify or
    // replace existing values. Comment out obsolete items.
    kMaxValue = kMatchEditDistance,
  };

  // Used for UKM. There is only a single MatchType per navigation.
  enum class MatchType {
    kNone = 0,
    kTopSite = 1,
    kSiteEngagement = 2,
    kEditDistance = 3,

    // Append new items to the end of the list above; do not modify or replace
    // existing values. Comment out obsolete items.
    kMaxValue = kEditDistance,
  };

  struct DomainInfo {
    const std::string domain_and_registry;
    const url_formatter::IDNConversionResult idn_result;
    const url_formatter::Skeletons skeletons;
    DomainInfo(const std::string& arg_domain_and_registry,
               const url_formatter::IDNConversionResult& arg_idn_result,
               const url_formatter::Skeletons& arg_skeletons);
    ~DomainInfo();
    DomainInfo(const DomainInfo& other);
  };

  static const char kHistogramName[];

  static void CreateForWebContents(content::WebContents* web_contents);

  explicit LookalikeUrlNavigationObserver(content::WebContents* web_contents);
  ~LookalikeUrlNavigationObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<LookalikeUrlNavigationObserver>;
  FRIEND_TEST_ALL_PREFIXES(LookalikeUrlNavigationObserverTest,
                           IsEditDistanceAtMostOne);

  DomainInfo GetDomainInfo(const GURL& url);

  // Performs top domain and engaged site checks on the navigated |url|. Uses
  // |engaged_sites| for the engaged site checks.
  void PerformChecks(const GURL& url,
                     const DomainInfo& navigated_domain,
                     const std::vector<GURL>& engaged_sites);

  // Returns true if a domain is visually similar to the hostname of |url|. The
  // matching domain can be a top domain or an engaged site. Similarity check
  // is made using both visual skeleton and edit distance comparison. If this
  // returns true, match details will be written into |matched_domain| and
  // |match_type|. They cannot be nullptr.
  bool GetMatchingDomain(const DomainInfo& navigated_domain,
                         const std::vector<GURL>& engaged_sites,
                         std::string* matched_domain,
                         MatchType* match_type);

  // Returns if the Levenshtein distance between |str1| and |str2| is at most 1.
  // This has O(max(n,m)) complexity as opposed to O(n*m) of the usual edit
  // distance computation.
  static bool IsEditDistanceAtMostOne(const base::string16& str1,
                                      const base::string16& str2);

  // Returns the first matching top domain with an edit distance of at most one
  // to |domain_and_registry|.
  static std::string GetSimilarDomainFromTop500(const DomainInfo& domain_info);

  Profile* profile_;
  base::WeakPtrFactory<LookalikeUrlNavigationObserver> weak_factory_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_OBSERVER_H_
