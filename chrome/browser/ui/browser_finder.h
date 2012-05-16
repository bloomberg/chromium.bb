// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_FINDER_H_
#define CHROME_BROWSER_UI_BROWSER_FINDER_H_
#pragma once

#include "chrome/browser/ui/browser.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace contents {
class NavigationController;
class WebContents;
}

// Collection of functions to find Browsers based on various criteria.

namespace browser {

// Retrieve the last active tabbed browser with a profile matching |profile|.
// If |match_original_profiles| is true, matching is done based on the
// original profile, eg profile->GetOriginalProfile() ==
// browser->profile()->GetOriginalProfile(). This has the effect of matching
// against both non-incognito and incognito profiles. If
// |match_original_profiles| is false, only an exact match may be returned.
Browser* FindTabbedBrowser(Profile* profile, bool match_original_profiles);

// Returns the first tabbed browser matching |profile|. If there is no tabbed
// browser a new one is created and returned. If a new browser is created it is
// not made visible.
Browser* FindOrCreateTabbedBrowser(Profile* profile);

// Find an existing browser window with any type. See comment above for
// additional information.
Browser* FindAnyBrowser(Profile* profile, bool match_original_profiles);

// Find an existing browser window that can provide the specified type (this
// uses Browser::CanSupportsWindowFeature, not
// Browser::SupportsWindowFeature). This searches in the order of last
// activation. Only browsers that have been active can be returned. Returns
// NULL if no such browser currently exists.
Browser* FindBrowserWithFeature(Profile* profile,
                                Browser::WindowFeature feature);

// Find an existing browser window with the provided profile. Searches in the
// order of last activation. Only browsers that have been active can be
// returned. Returns NULL if no such browser currently exists.
Browser* FindBrowserWithProfile(Profile* profile);

// Find an existing browser with the provided ID. Returns NULL if no such
// browser currently exists.
Browser* FindBrowserWithID(SessionID::id_type desired_id);

// Find the browser represented by |window| or NULL if not found.
Browser* FindBrowserWithWindow(gfx::NativeWindow window);

// Find the browser containing |web_contents| or NULL if none is found.
// |web_contents| must not be NULL.
Browser* FindBrowserWithWebContents(content::WebContents* web_contents);

// Returns the Browser which contains the tab with the given
// NavigationController, also filling in |index| (if valid) with the tab's index
// in the tab strip.  Returns NULL if not found.  This call is O(N) in the
// number of tabs.
Browser* FindBrowserForController(
    const content::NavigationController* controller,
    int* index);

// Identical in behavior to BrowserList::GetLastActive(), except that the most
// recently open browser owned by |profile| is returned. If none exist, returns
// NULL.  WARNING: see warnings in BrowserList::GetLastActive().
Browser* FindLastActiveWithProfile(Profile* profile);

// Returns the number of browsers with the Profile |profile|.
size_t GetBrowserCount(Profile* profile);

// Returns the number of tabbed browsers with the Profile |profile|.
size_t GetTabbedBrowserCount(Profile* profile);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_BROWSER_FINDER_H_
