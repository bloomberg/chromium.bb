// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_TERMS_TRACKER_H_
#define CHROME_BROWSER_SEARCH_SEARCH_TERMS_TRACKER_H_

#include <map>

#include "base/strings/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class NavigationController;
class WebContents;
}

namespace chrome {

// Observes navigation events (and WebContents destructions) to keep track of
// search terms associated with a WebContents. Essentially, as long as there are
// only web-triggerable navigations following a search results page, this class
// will consider the search terms from that SRP as the "current" search terms.
// Any other type of navigation will invalidate the search terms. The search
// terms are being tracked so they can be displayed in the location bar for
// related navigations that occur after a search.
class SearchTermsTracker : public content::NotificationObserver {
 public:
  SearchTermsTracker();
  virtual ~SearchTermsTracker();

  // Returns the current search terms and navigation index of the corresponding
  // search results page for the specified WebContents. This function will
  // return true if there are valid search terms for |contents|. |search_terms|
  // and/or |navigation_index| can be NULL if not needed.
  bool GetSearchTerms(const content::WebContents* contents,
                      base::string16* search_terms,
                      int* navigation_index) const;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  struct TabData {
    TabData() : srp_navigation_index(-1) {}
    base::string16 search_terms;
    int srp_navigation_index;
  };

  // Keeps information about the specified WebContents.
  typedef std::map<const content::WebContents*, TabData> TabState;

  // Searches for the most recent search and, if found, fills |tab_data| with
  // information about that search and returns true.
  bool FindMostRecentSearch(const content::NavigationController* controller,
                            TabData* tab_data);

  // Removes the TabData entry associated to the specified |contents|.
  void RemoveTabData(const content::WebContents* contents);

  content::NotificationRegistrar registrar_;
  TabState tabs_;

  DISALLOW_COPY_AND_ASSIGN(SearchTermsTracker);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SEARCH_SEARCH_TERMS_TRACKER_H_
