// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_SERVICE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class InstantIOContext;
class Profile;

namespace net {
class URLRequest;
}

// Tracks render process host IDs that are associated with Instant.
class InstantService : public ProfileKeyedService,
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

  // If |url| is known the existing Most Visited item ID is returned.  Otherwise
  // a new Most Visited item ID is associated with the |url| and returned.
  uint64 AddURL(const GURL& url);

  // If there is a mapping for the |url|, sets |most_visited_item_id| and
  // returns true.
  bool GetMostVisitedItemIDForURL(const GURL& url,
                                  uint64* most_visited_item_id);

  // If there is a mapping for the |most_visited_item_id|, sets |url| and
  // returns true.
  bool GetURLForMostVisitedItemId(uint64 most_visited_item_id, GURL* url);

 private:
  // Overridden from ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Removes entries of each url in |deleted_urls| from the ID maps.  Or all
  // IDs if |all_history| is true.  |deleted_ids| is filled with the newly
  // deleted Most Visited item IDs.
  void DeleteHistoryURLs(const std::vector<GURL>& deleted_urls,
                         std::vector<uint64>* deleted_ids);

  Profile* const profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  // A mapping of Most Visited IDs to URLs.  Used to hide Most Visited and
  // Favicon URLs from the Instant search provider.
  uint64 last_most_visited_item_id_;
  std::map<uint64, GURL> most_visited_item_id_to_url_map_;
  std::map<GURL, uint64> url_to_most_visited_item_id_map_;

  content::NotificationRegistrar registrar_;

  scoped_refptr<InstantIOContext> instant_io_context_;

  DISALLOW_COPY_AND_ASSIGN(InstantService);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_SERVICE_H_
