// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#pragma once

#include <list>

#include "base/non_thread_safe.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

class PrerenderContents;
class Profile;
class TabContents;

// PrerenderManager is responsible for initiating and keeping prerendered
// views of webpages.
class PrerenderManager : NonThreadSafe {
 public:
  // Owned by a Profile object for the lifetime of the profile.
  explicit PrerenderManager(Profile* profile);
  virtual ~PrerenderManager();

  // Preloads the URL supplied.
  void AddPreload(const GURL& url);

  // For a given TabContents that wants to navigate to the URL supplied,
  // determines whether a preloaded version of the URL can be used,
  // and substitutes the prerendered RVH into the TabContents.  Returns
  // whether or not a prerendered RVH could be used or not.
  bool MaybeUsePreloadedPage(TabContents* tc, const GURL& url);

  // Allows PrerenderContents to remove itself when prerendering should
  // be cancelled.  Also deletes the entry.
  void RemoveEntry(PrerenderContents* entry);

  // Retrieves the PrerenderContents object for the specified URL, if it
  // has been prerendered.  The caller will then have ownership of the
  // PrerenderContents object and is responsible for freeing it.
  // Returns NULL if the specified URL has not been prerendered.
  PrerenderContents* GetEntry(const GURL& url);

  base::TimeDelta max_prerender_age() const { return max_prerender_age_; }
  void set_max_prerender_age(base::TimeDelta td) { max_prerender_age_ = td; }
  unsigned int max_elements() const { return max_elements_; }
  void set_max_elements(unsigned int num) { max_elements_ = num; }

 protected:
  // The following methods exist explicitly rather than just inlined to
  // facilitate testing.
  virtual base::Time GetCurrentTime() const;
  virtual PrerenderContents* CreatePrerenderContents(const GURL& url);

 private:
  struct PrerenderContentsData;

  bool IsPrerenderElementFresh(const base::Time start) const;
  void DeleteOldEntries();

  Profile* profile_;

  base::TimeDelta max_prerender_age_;
  unsigned int max_elements_;

  // List of prerendered elements.
  std::list<PrerenderContentsData> prerender_list_;

  // Default maximum permitted elements to prerender.
  static const unsigned int kDefaultMaxPrerenderElements = 1;

  // Default maximum age a prerendered element may have, in seconds.
  static const int kDefaultMaxPrerenderAgeSeconds = 20;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
