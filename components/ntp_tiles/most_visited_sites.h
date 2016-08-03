// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
#define COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "url/gurl.h"

namespace history {
class TopSites;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ntp_tiles {

// Shim interface for SupervisedUserService.
class MostVisitedSitesSupervisor {
 public:
  struct Whitelist {
    base::string16 title;
    GURL entry_point;
    base::FilePath large_icon_path;
  };

  class Observer {
   public:
    virtual void OnBlockedSitesChanged() = 0;

   protected:
    ~Observer() {}
  };

  // Pass non-null to set observer, or null to remove observer.
  // If setting observer, there must not yet be an observer set.
  // If removing observer, there must already be one to remove.
  // Does not take ownership. Observer must outlive this object.
  virtual void SetObserver(Observer* new_observer) = 0;

  // If true, |url| should not be shown on the NTP.
  virtual bool IsBlocked(const GURL& url) = 0;

  // Explicit suggestions for sites to show on NTP.
  virtual std::vector<Whitelist> whitelists() = 0;

  // If true, be conservative about suggesting sites from outside sources.
  virtual bool IsChildProfile() = 0;

 protected:
  virtual ~MostVisitedSitesSupervisor() {}
};

// Tracks the list of most visited sites and their thumbnails.
//
// Do not use, except from MostVisitedSitesBridge. The interface is in flux
// while we are extracting the functionality of the Java class to make available
// in C++.
//
// TODO(sfiera): finalize interface.
class MostVisitedSites : public history::TopSitesObserver,
                         public MostVisitedSitesSupervisor::Observer {
 public:
  struct Suggestion;
  using SuggestionsVector = std::vector<Suggestion>;
  using PopularSitesVector = std::vector<PopularSites::Site>;

  // The visual type of a most visited tile.
  //
  // These values must stay in sync with the MostVisitedTileType enum
  // in histograms.xml.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp
  enum MostVisitedTileType {
    // The icon or thumbnail hasn't loaded yet.
    NONE,
    // The item displays a site's actual favicon or touch icon.
    ICON_REAL,
    // The item displays a color derived from the site's favicon or touch icon.
    ICON_COLOR,
    // The item displays a default gray box in place of an icon.
    ICON_DEFAULT,
    NUM_TILE_TYPES,
  };

  // The source of the Most Visited sites.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp
  enum MostVisitedSource {
    // Item comes from the personal top sites list.
    TOP_SITES,
    // Item comes from the suggestions service.
    SUGGESTIONS_SERVICE,
    // Item is regionally popular.
    POPULAR,
    // Item is on an custodian-managed whitelist.
    WHITELIST
  };

  // The observer to be notified when the list of most visited sites changes.
  class Observer {
   public:
    virtual void OnMostVisitedURLsAvailable(
        const SuggestionsVector& suggestions) = 0;
    virtual void OnPopularURLsAvailable(const PopularSitesVector& sites) = 0;

   protected:
    virtual ~Observer() {}
  };

  struct Suggestion {
    base::string16 title;
    GURL url;
    MostVisitedSource source;

    // Only valid for source == WHITELIST (empty otherwise).
    base::FilePath whitelist_icon_path;

    Suggestion();
    ~Suggestion();

    Suggestion(Suggestion&&);
    Suggestion& operator=(Suggestion&&);

   private:
    DISALLOW_COPY_AND_ASSIGN(Suggestion);
  };

  MostVisitedSites(PrefService* prefs,
                   scoped_refptr<history::TopSites> top_sites,
                   suggestions::SuggestionsService* suggestions,
                   PopularSites* popular_sites,
                   MostVisitedSitesSupervisor* supervisor);

  ~MostVisitedSites() override;

  // Does not take ownership of |observer|, which must outlive this object and
  // must not be null.
  void SetMostVisitedURLsObserver(Observer* observer, int num_sites);

  void AddOrRemoveBlacklistedUrl(const GURL& url, bool add_url);
  void RecordTileTypeMetrics(const std::vector<int>& tile_types,
                             const std::vector<int>& sources);
  void RecordOpenedMostVisitedItem(int index, int tile_type, int source);

  // MostVisitedSitesSupervisor::Observer implementation.
  void OnBlockedSitesChanged() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class MostVisitedSitesTest;

  void BuildCurrentSuggestions();

  // Initialize the query to Top Sites. Called if the SuggestionsService
  // returned no data.
  void InitiateTopSitesQuery();

  // If there's a whitelist entry point for the URL, return the large icon path.
  base::FilePath GetWhitelistLargeIconPath(const GURL& url);

  // Callback for when data is available from TopSites.
  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& visited_list);

  // Callback for when data is available from the SuggestionsService.
  void OnSuggestionsProfileAvailable(
      const suggestions::SuggestionsProfile& suggestions_profile);

  // Takes the personal suggestions and creates whitelist entry point
  // suggestions if necessary.
  SuggestionsVector CreateWhitelistEntryPointSuggestions(
      const SuggestionsVector& personal_suggestions);

  // Takes the personal and whitelist suggestions and creates popular
  // suggestions if necessary.
  SuggestionsVector CreatePopularSitesSuggestions(
      const SuggestionsVector& personal_suggestions,
      const SuggestionsVector& whitelist_suggestions);

  // Takes the personal suggestions, creates and merges in whitelist and popular
  // suggestions if appropriate, and saves the new suggestions.
  void SaveNewSuggestions(SuggestionsVector personal_suggestions);

  // Workhorse for SaveNewSuggestions above. Implemented as a separate static
  // method for ease of testing.
  static SuggestionsVector MergeSuggestions(
      SuggestionsVector personal_suggestions,
      SuggestionsVector whitelist_suggestions,
      SuggestionsVector popular_suggestions);

  // Notifies the observer about the availability of suggestions.
  // Also records impressions UMA if not done already.
  void NotifyMostVisitedURLsObserver();

  void OnPopularSitesAvailable(bool success);

  // Records UMA histogram metrics related to the number of impressions.
  void RecordImpressionUMAMetrics();

  // history::TopSitesObserver implementation.
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  PrefService* prefs_;
  scoped_refptr<history::TopSites> top_sites_;
  suggestions::SuggestionsService* suggestions_service_;
  PopularSites* const popular_sites_;
  MostVisitedSitesSupervisor* supervisor_;

  Observer* observer_;

  // The maximum number of most visited sites to return.
  int num_sites_;

  // True if we are still waiting for an initial set of most visited sites (from
  // either TopSites or the SuggestionsService).
  bool waiting_for_most_visited_sites_;

  // True if we are still waiting for the set of popular sites. Immediately set
  // to false if popular sites are disabled, or are not required.
  bool waiting_for_popular_sites_;

  // True if we have recorded one-shot UMA metrics such as impressions. They are
  // recorded once both the previous flags are true.
  bool recorded_uma_;

  std::unique_ptr<
      suggestions::SuggestionsService::ResponseCallbackList::Subscription>
      suggestions_subscription_;

  ScopedObserver<history::TopSites, history::TopSitesObserver>
      top_sites_observer_;

  // The main source of personal suggestions - either TOP_SITES or
  // SUGGESTIONS_SEVICE.
  MostVisitedSource mv_source_;

  SuggestionsVector current_suggestions_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<MostVisitedSites> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSites);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_MOST_VISITED_SITES_H_
