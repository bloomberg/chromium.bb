// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_

#include <memory>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

class InstantIOContext;
struct InstantMostVisitedItem;
class InstantSearchPrerenderer;
class InstantServiceObserver;
class Profile;
struct TemplateURLData;
class TemplateURLService;
struct ThemeBackgroundInfo;

namespace content {
class RenderProcessHost;
}

// Tracks render process host IDs that are associated with Instant.
class InstantService : public KeyedService,
                       public content::NotificationObserver,
                       public TemplateURLServiceObserver,
                       public history::TopSitesObserver {
 public:
  explicit InstantService(Profile* profile);
  ~InstantService() override;

  // Add, remove, and query RenderProcessHost IDs that are associated with
  // Instant processes.
  void AddInstantProcess(int process_id);
  bool IsInstantProcess(int process_id) const;

  // Adds/Removes InstantService observers.
  void AddObserver(InstantServiceObserver* observer);
  void RemoveObserver(InstantServiceObserver* observer);

#if defined(UNIT_TEST)
  int GetInstantProcessCount() const {
    return process_ids_.size();
  }
#endif

  // Invoked whenever an NTP is opened. Causes an async refresh of Most Visited
  // items.
  void OnNewTabPageOpened();

  // Most visited item API.

  // Invoked when the Instant page wants to delete a Most Visited item.
  void DeleteMostVisitedItem(const GURL& url);

  // Invoked when the Instant page wants to undo the deletion.
  void UndoMostVisitedDeletion(const GURL& url);

  // Invoked when the Instant page wants to undo all Most Visited deletions.
  void UndoAllMostVisitedDeletions();

  // Invoked by the InstantController to update theme information for NTP.
  //
  // TODO(kmadhusu): Invoking this from InstantController shouldn't be
  // necessary. Investigate more and remove this from here.
  void UpdateThemeInfo();

  // Invoked by the InstantController to update most visited items details for
  // NTP.
  void UpdateMostVisitedItemsInfo();

  // Sends the current set of search URLs to a renderer process.
  void SendSearchURLsToRenderer(content::RenderProcessHost* rph);

  InstantSearchPrerenderer* GetInstantSearchPrerenderer();

 private:
  friend class InstantExtendedTest;
  friend class InstantServiceTest;
  friend class InstantTestBase;
  friend class InstantUnitTestBase;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedManualTest,
                           MANUAL_SearchesFromFakebox);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ProcessIsolation);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceEnabledTest,
                           SendsSearchURLsToRenderer);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, GetSuggestionFromServiceSide);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, GetSuggestionFromClientSide);

  // KeyedService:
  void Shutdown() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // TemplateURLServiceObserver:
  // Caches the previous value of the Default Search Provider and the Google
  // base URL to filter out changes other than those affecting the Default
  // Search Provider.
  void OnTemplateURLServiceChanged() override;

  // TopSitesObserver:
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  // Called when a renderer process is terminated.
  void OnRendererProcessTerminated(int process_id);

  // Called when we get new most visited items from TopSites, registered as an
  // async callback. Parses them and sends them to the renderer via
  // NotifyAboutMostVisitedItems.
  void OnMostVisitedItemsReceived(const history::MostVisitedURLList& data);

  // Notifies the observer about the last known most visited items.
  void NotifyAboutMostVisitedItems();

#if defined(ENABLE_THEMES)
  // Theme changed notification handler.
  void OnThemeChanged();
#endif

  void ResetInstantSearchPrerendererIfNecessary();

  Profile* const profile_;

  // The TemplateURLService that we are observing. It will outlive this
  // InstantService due to the dependency declared in InstantServiceFactory.
  TemplateURLService* template_url_service_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // InstantMostVisitedItems from TopSites.
  std::vector<InstantMostVisitedItem> most_visited_items_;

  // Theme-related data for NTP overlay to adopt themes.
  std::unique_ptr<ThemeBackgroundInfo> theme_info_;

  base::ObserverList<InstantServiceObserver> observers_;

  content::NotificationRegistrar registrar_;

  scoped_refptr<InstantIOContext> instant_io_context_;

  // Set to NULL if the default search provider does not support Instant.
  std::unique_ptr<InstantSearchPrerenderer> instant_prerenderer_;

  // Used to check whether notifications from TemplateURLService indicate a
  // change that affects the default search provider.
  std::unique_ptr<TemplateURLData> previous_default_search_provider_;
  GURL previous_google_base_url_;

  // Used for Top Sites async retrieval.
  base::WeakPtrFactory<InstantService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
