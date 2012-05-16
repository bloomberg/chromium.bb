// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"

using content::WebContents;

namespace browser {

namespace {

// Type used to indicate to match anything.
const int kMatchAny                     = 0;

// See BrowserMatches for details.
const int kMatchOriginalProfile         = 1 << 0;
const int kMatchCanSupportWindowFeature = 1 << 1;
const int kMatchTabbed                  = 1 << 2;

// Returns true if the specified |browser| matches the specified arguments.
// |match_types| is a bitmask dictating what parameters to match:
// . If it contains kMatchOriginalProfile then the original profile of the
//   browser must match |profile->GetOriginalProfile()|. This is used to match
//   incognito windows.
// . If it contains kMatchCanSupportWindowFeature
//   |CanSupportWindowFeature(window_feature)| must return true.
// . If it contains kMatchTabbed, the browser must be a tabbed browser.
bool BrowserMatches(Browser* browser,
                    Profile* profile,
                    Browser::WindowFeature window_feature,
                    uint32 match_types) {
  if (match_types & kMatchCanSupportWindowFeature &&
      !browser->CanSupportWindowFeature(window_feature)) {
    return false;
  }

  if (match_types & kMatchOriginalProfile) {
    if (browser->profile()->GetOriginalProfile() !=
        profile->GetOriginalProfile())
      return false;
  } else if (browser->profile() != profile) {
    return false;
  }

  if (match_types & kMatchTabbed)
    return browser->is_type_tabbed();

  return true;
}

// Returns the first browser in the specified iterator that returns true from
// |BrowserMatches|, or null if no browsers match the arguments. See
// |BrowserMatches| for details on the arguments.
template <class T>
Browser* FindBrowserMatching(const T& begin,
                             const T& end,
                             Profile* profile,
                             Browser::WindowFeature window_feature,
                             uint32 match_types) {
  for (T i = begin; i != end; ++i) {
    if (BrowserMatches(*i, profile, window_feature, match_types))
      return *i;
  }
  return NULL;
}

Browser* FindBrowserWithTabbedOrAnyType(Profile* profile,
                                        bool match_tabbed,
                                        bool match_original_profiles) {
  uint32 match_types = kMatchAny;
  if (match_tabbed)
    match_types |= kMatchTabbed;
  if (match_original_profiles)
    match_types |= kMatchOriginalProfile;
  Browser* browser = FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(),
      profile, Browser::FEATURE_NONE, match_types);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser :
      FindBrowserMatching(BrowserList::begin(), BrowserList::end(), profile,
                          Browser::FEATURE_NONE, match_types);
}

size_t GetBrowserCountImpl(Profile* profile, uint32 match_types) {
  size_t count = 0;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (BrowserMatches(*i, profile, Browser::FEATURE_NONE, match_types))
      count++;
  }
  return count;
}

}  // namespace

Browser* FindTabbedBrowser(Profile* profile, bool match_original_profiles) {
  return FindBrowserWithTabbedOrAnyType(profile,
                                        true,
                                        match_original_profiles);
}

Browser* FindOrCreateTabbedBrowser(Profile* profile) {
  Browser* browser = FindTabbedBrowser(profile, false);
  if (!browser)
    browser = Browser::Create(profile);
  return browser;
}

Browser* FindAnyBrowser(Profile* profile, bool match_original_profiles) {
  return FindBrowserWithTabbedOrAnyType(profile,
                                        false,
                                        match_original_profiles);
}

Browser* FindBrowserWithFeature(Profile* profile,
                                Browser::WindowFeature feature) {
  Browser* browser = FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(),
      profile, feature, kMatchCanSupportWindowFeature);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser :
      FindBrowserMatching(BrowserList::begin(), BrowserList::end(), profile,
                          feature, kMatchCanSupportWindowFeature);
}

Browser* FindBrowserWithProfile(Profile* profile) {
  return FindAnyBrowser(profile, false);
}

Browser* FindBrowserWithID(SessionID::id_type desired_id) {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->session_id().id() == desired_id)
      return *i;
  }
  return NULL;
}

Browser* FindBrowserWithWindow(gfx::NativeWindow window) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    Browser* browser = *it;
    if (browser->window() && browser->window()->GetNativeHandle() == window)
      return browser;
  }
  return NULL;
}

Browser* FindBrowserWithWebContents(WebContents* web_contents) {
  DCHECK(web_contents);
  for (TabContentsIterator it; !it.done(); ++it) {
    if (it->web_contents() == web_contents)
      return it.browser();
  }
  return NULL;
}

Browser* FindBrowserForController(
    const content::NavigationController* controller,
    int* index_result) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    int index = (*it)->GetIndexOfController(controller);
    if (index != TabStripModel::kNoTab) {
      if (index_result)
        *index_result = index;
      return *it;
    }
  }
  return NULL;
}


Browser* FindLastActiveWithProfile(Profile* profile) {
  // We are only interested in last active browsers, so we don't fall back to
  // all browsers like FindBrowserWith* do.
  return FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(), profile,
      Browser::FEATURE_NONE, kMatchAny);
}

size_t GetBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kMatchAny);
}

size_t GetTabbedBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kMatchTabbed);
}

}  // namespace browser
