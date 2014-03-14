// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/common/instant_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class InstantIOContext;
class InstantServiceObserver;
class InstantTestBase;
class InstantServiceTest;
class Profile;
class ThemeService;

namespace content {
class RenderProcessHost;
}

namespace net {
class URLRequest;
}

// Tracks render process host IDs that are associated with Instant.
class InstantService : public KeyedService,
                       public content::NotificationObserver {
 public:
  explicit InstantService(Profile* profile);
  virtual ~InstantService();

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

  // Most visited item API.

  // Invoked by the InstantController when the Instant page wants to delete a
  // Most Visited item.
  void DeleteMostVisitedItem(const GURL& url);

  // Invoked by the InstantController when the Instant page wants to undo the
  // blacklist action.
  void UndoMostVisitedDeletion(const GURL& url);

  // Invoked by the InstantController when the Instant page wants to undo all
  // Most Visited deletions.
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

  // Invoked to notify the Instant page that the omnibox start margin has
  // changed.
  void OnOmniboxStartMarginChanged(int start_margin);

  InstantSearchPrerenderer* instant_search_prerenderer() {
    return instant_prerenderer_.get();
  }

  int omnibox_start_margin() const { return omnibox_start_margin_; }

 private:
  friend class InstantExtendedTest;
  friend class InstantServiceTest;
  friend class InstantTestBase;
  friend class InstantUnitTestBase;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedManualTest,
                           MANUAL_SearchesFromFakebox);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ProcessIsolation);
  FRIEND_TEST_ALL_PREFIXES(InstantServiceTest, SendsSearchURLsToRenderer);

  // Overridden from KeyedService:
  virtual void Shutdown() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when a renderer process is terminated.
  void OnRendererProcessTerminated(int process_id);

  // Called when we get new most visited items from TopSites, registered as an
  // async callback. Parses them and sends them to the renderer via
  // SendMostVisitedItems.
  void OnMostVisitedItemsReceived(const history::MostVisitedURLList& data);

  // Notifies the observer about the last known most visited items.
  void NotifyAboutMostVisitedItems();

  // Theme changed notification handler.
  void OnThemeChanged(ThemeService* theme_service);

  void OnGoogleURLUpdated(Profile* profile,
                          GoogleURLTracker::UpdatedDetails* details);

  void OnDefaultSearchProviderChanged(const std::string& pref_name);

  void ResetInstantSearchPrerenderer();

  Profile* const profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // InstantMostVisitedItems sent to the Instant Pages.
  std::vector<InstantMostVisitedItem> most_visited_items_;

  // Theme-related data for NTP overlay to adopt themes.
  scoped_ptr<ThemeBackgroundInfo> theme_info_;

  // The start-edge margin of the omnibox, used by the Instant page to align
  // text or assets properly with the omnibox.
  int omnibox_start_margin_;

  ObserverList<InstantServiceObserver> observers_;

  content::NotificationRegistrar registrar_;

  PrefChangeRegistrar profile_pref_registrar_;

  scoped_refptr<InstantIOContext> instant_io_context_;

  // Set to NULL if the default search provider does not support Instant.
  scoped_ptr<InstantSearchPrerenderer> instant_prerenderer_;

  // Used for Top Sites async retrieval.
  base::WeakPtrFactory<InstantService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
