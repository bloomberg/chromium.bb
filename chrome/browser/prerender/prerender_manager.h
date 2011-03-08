// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#pragma once

#include <list>
#include <vector>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "googleurl/src/gurl.h"

class Profile;
class TabContents;

#if defined(COMPILER_GCC)

namespace __gnu_cxx {
template <>
struct hash<TabContents*> {
  std::size_t operator()(TabContents* value) const {
    return reinterpret_cast<std::size_t>(value);
  }
};
}

#endif

namespace prerender {

// PrerenderManager is responsible for initiating and keeping prerendered
// views of webpages.
class PrerenderManager : public base::RefCounted<PrerenderManager> {
 public:
  // PrerenderManagerMode is used in a UMA_HISTOGRAM, so please do not
  // add in the middle.
  enum PrerenderManagerMode {
    PRERENDER_MODE_DISABLED,
    PRERENDER_MODE_ENABLED,
    PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP,
    PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP,
    PRERENDER_MODE_MAX
  };

  // Owned by a Profile object for the lifetime of the profile.
  explicit PrerenderManager(Profile* profile);

  // Preloads the URL supplied.  alias_urls indicates URLs that redirect
  // to the same URL to be preloaded. Returns true if the URL was added,
  // false if it was not.
  bool AddPreload(const GURL& url, const std::vector<GURL>& alias_urls,
                  const GURL& referrer);

  // For a given TabContents that wants to navigate to the URL supplied,
  // determines whether a preloaded version of the URL can be used,
  // and substitutes the prerendered RVH into the TabContents.  Returns
  // whether or not a prerendered RVH could be used or not.
  bool MaybeUsePreloadedPage(TabContents* tc, const GURL& url);

  // Allows PrerenderContents to remove itself when prerendering should
  // be cancelled.
  void RemoveEntry(PrerenderContents* entry);

  // Retrieves the PrerenderContents object for the specified URL, if it
  // has been prerendered.  The caller will then have ownership of the
  // PrerenderContents object and is responsible for freeing it.
  // Returns NULL if the specified URL has not been prerendered.
  PrerenderContents* GetEntry(const GURL& url);

  // The following two methods should only be called from the UI thread.
  static void RecordPerceivedPageLoadTime(base::TimeDelta pplt);
  void RecordTimeUntilUsed(base::TimeDelta time_until_used);

  base::TimeDelta max_prerender_age() const { return max_prerender_age_; }
  void set_max_prerender_age(base::TimeDelta td) { max_prerender_age_ = td; }
  unsigned int max_elements() const { return max_elements_; }
  void set_max_elements(unsigned int num) { max_elements_ = num; }

  static PrerenderManagerMode GetMode();
  static void SetMode(PrerenderManagerMode mode);
  static bool IsPrerenderingEnabled();

  // The following static method can be called from any thread, but will result
  // in posting a task to the UI thread if we are not in the UI thread.
  static void RecordPrefetchTagObserved();

  // Maintaining and querying the set of TabContents belonging to this
  // PrerenderManager that are currently showing prerendered pages.
  void MarkTabContentsAsPrerendered(TabContents* tc);
  void MarkTabContentsAsNotPrerendered(TabContents* tc);
  bool IsTabContentsPrerendered(TabContents* tc) const;

 protected:
  virtual ~PrerenderManager();

  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);

 private:
  // Test that needs needs access to internal functions.
  friend class PrerenderBrowserTest;

  friend class base::RefCounted<PrerenderManager>;
  struct PrerenderContentsData;

  // Starts and stops scheduling periodic cleanups, respectively.
  void StartSchedulingPeriodicCleanups();
  void StopSchedulingPeriodicCleanups();

  // Deletes stale prerendered PrerenderContents.
  // Also identifies and kills PrerenderContents that use too much
  // resources.
  void PeriodicCleanup();

  bool IsPrerenderElementFresh(const base::Time start) const;
  void DeleteOldEntries();
  virtual base::Time GetCurrentTime() const;
  virtual PrerenderContents* CreatePrerenderContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      const GURL& referrer);

  // Finds the specified PrerenderContents and returns it, if it exists.
  // Returns NULL otherwise.  Unlike GetEntry, the PrerenderManager maintains
  // ownership of the PrerenderContents.
  PrerenderContents* FindEntry(const GURL& url);

  static bool ShouldRecordWindowedPPLT();

  static void RecordPrefetchTagObservedOnUIThread();

  Profile* profile_;

  base::TimeDelta max_prerender_age_;
  unsigned int max_elements_;

  // List of prerendered elements.
  std::list<PrerenderContentsData> prerender_list_;

  // Set of TabContents which are currently displaying a prerendered page.
  base::hash_set<TabContents*> prerendered_tc_set_;

  // Default maximum permitted elements to prerender.
  static const unsigned int kDefaultMaxPrerenderElements = 1;

  // Default maximum age a prerendered element may have, in seconds.
  static const int kDefaultMaxPrerenderAgeSeconds = 20;

  // Time window for which we will record windowed PLT's from the last
  // observed link rel=prefetch tag.
  static const int kWindowedPPLTSeconds = 30;

  // Time interval at which periodic cleanups are performed.
  static const int kPeriodicCleanupIntervalMs = 1000;

  scoped_ptr<PrerenderContents::Factory> prerender_contents_factory_;

  static PrerenderManagerMode mode_;

  // The time when we last saw a prefetch request coming from a renderer.
  // This is used to record perceived PLT's for a certain amount of time
  // from the point that we last saw a <link rel=prefetch> tag.
  // This static variable should only be modified on the UI thread.
  static base::TimeTicks last_prefetch_seen_time_;

  // RepeatingTimer to perform periodic cleanups of pending prerendered
  // pages.
  base::RepeatingTimer<PrerenderManager> repeating_timer_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

}  // prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
