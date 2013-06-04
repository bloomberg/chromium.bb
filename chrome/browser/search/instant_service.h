// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/instant_restricted_id_cache.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class InstantIOContext;
class Profile;

namespace net {
class URLRequest;
}

// Tracks render process host IDs that are associated with Instant.
class InstantService : public BrowserContextKeyedService,
                       public content::NotificationObserver {
 public:
  explicit InstantService(Profile* profile);
  virtual ~InstantService();

  // A utility to translate an Instant path if it is of Most Visited item ID
  // form.  If path is a Most Visited item ID and we have a URL for it, then
  // this URL is returned in string form.  The |path| is a URL fragment
  // corresponding to the path of url with the leading slash ("/") stripped.
  // For example, chrome-search://favicon/72 would yield a |path| value of "72",
  // and since 72 is a valid uint64 the path is translated to a valid url,
  // "http://bingo.com/", say.
  static const std::string MaybeTranslateInstantPathOnUI(
      Profile* profile, const std::string& path);
  static const std::string MaybeTranslateInstantPathOnIO(
    const net::URLRequest* request, const std::string& path);
  static bool IsInstantPath(const GURL& url);

  // Add, remove, and query RenderProcessHost IDs that are associated with
  // Instant processes.
  void AddInstantProcess(int process_id);
  bool IsInstantProcess(int process_id) const;

#if defined(UNIT_TEST)
  int GetInstantProcessCount() const {
    return process_ids_.size();
  }
#endif

  // Most visited item API.

  // Adds |items| to the |most_visited_item_cache_| assigning restricted IDs in
  // the process.
  void AddMostVisitedItems(const std::vector<InstantMostVisitedItem>& items);

  // Invoked by the InstantController when the Instant page wants to delete a
  // Most Visited item.
  void DeleteMostVisitedItem(const GURL& url);

  // Invoked by the InstantController when the Instant page wants to undo the
  // blacklist action.
  void UndoMostVisitedDeletion(const GURL& url);

  // Invoked by the InstantController when the Instant page wants to undo all
  // Most Visited deletions.
  void UndoAllMostVisitedDeletions();

  // Returns the last added InstantMostVisitedItems. After the call to
  // |AddMostVisitedItems|, the caller should call this to get the items with
  // the assigned IDs.
  void GetCurrentMostVisitedItems(
      std::vector<InstantMostVisitedItemIDPair>* items) const;

 private:
  // Overridden from BrowserContextKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // If the |most_visited_item_id| is found in the cache, sets the |item| to it
  // and returns true.
  bool GetMostVisitedItemForID(InstantRestrictedID most_visited_item_id,
                               InstantMostVisitedItem* item) const;

  // Called when we get new most visited items from TopSites, registered as an
  // async callback. Parses them and sends them to the renderer via
  // SendMostVisitedItems.
  void OnMostVisitedItemsReceived(const history::MostVisitedURLList& data);

  Profile* const profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // A cache of the InstantMostVisitedItems sent to the Instant Pages.
  InstantRestrictedIDCache<InstantMostVisitedItem> most_visited_item_cache_;

  content::NotificationRegistrar registrar_;

  scoped_refptr<InstantIOContext> instant_io_context_;

  // Used for Top Sites async retrieval.
  base::WeakPtrFactory<InstantService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
