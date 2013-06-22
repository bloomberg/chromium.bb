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
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/instant_types.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class InstantIOContext;
class InstantServiceObserver;
class Profile;
class ThemeService;

namespace net {
class URLRequest;
}

// Tracks render process host IDs that are associated with Instant.
class InstantService : public BrowserContextKeyedService,
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

  // Returns the last added InstantMostVisitedItems.
  void GetCurrentMostVisitedItems(
      std::vector<InstantMostVisitedItem>* items) const;

  // Invoked by the InstantController to update theme information for NTP.
  //
  // TODO(kmadhusu): Invoking this from InstantController shouldn't be
  // necessary. Investigate more and remove this from here.
  void UpdateThemeInfo();

 private:
  // Overridden from BrowserContextKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when we get new most visited items from TopSites, registered as an
  // async callback. Parses them and sends them to the renderer via
  // SendMostVisitedItems.
  void OnMostVisitedItemsReceived(const history::MostVisitedURLList& data);

  // Theme changed notification handler.
  void OnThemeChanged(ThemeService* theme_service);

  Profile* const profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // InstantMostVisitedItems sent to the Instant Pages.
  std::vector<InstantMostVisitedItem> most_visited_items_;

  // Theme-related data for NTP overlay to adopt themes.
  scoped_ptr<ThemeBackgroundInfo> theme_info_;

  ObserverList<InstantServiceObserver> observers_;

  content::NotificationRegistrar registrar_;

  scoped_refptr<InstantIOContext> instant_io_context_;

  // Used for Top Sites async retrieval.
  base::WeakPtrFactory<InstantService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
