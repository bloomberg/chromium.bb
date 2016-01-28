// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include <stdint.h>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "components/signin/core/account_id/account_id.h"
#endif

using content::WebContents;

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
                    uint32_t match_types) {
  if ((match_types & kMatchCanSupportWindowFeature) &&
      !browser->CanSupportWindowFeature(window_feature)) {
    return false;
  }

#if defined(OS_CHROMEOS)
  // Get the profile on which the window is currently shown.
  // MultiUserWindowManager might be NULL under test scenario.
  chrome::MultiUserWindowManager* const window_manager =
      chrome::MultiUserWindowManager::GetInstance();
  Profile* shown_profile = nullptr;
  if (window_manager) {
    const AccountId& shown_account_id = window_manager->GetUserPresentingWindow(
        browser->window()->GetNativeWindow());
    shown_profile =
        shown_account_id.is_valid()
            ? multi_user_util::GetProfileFromAccountId(shown_account_id)
            : nullptr;
  }
#endif

  if (match_types & kMatchOriginalProfile) {
    if (browser->profile()->GetOriginalProfile() !=
        profile->GetOriginalProfile())
      return false;
#if defined(OS_CHROMEOS)
    if (shown_profile &&
        shown_profile->GetOriginalProfile() != profile->GetOriginalProfile()) {
      return false;
    }
#endif
  } else {
    if (browser->profile() != profile)
      return false;
#if defined(OS_CHROMEOS)
    if (shown_profile && shown_profile != profile)
      return false;
#endif
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
                             uint32_t match_types) {
  for (T i = begin; i != end; ++i) {
    if (BrowserMatches(*i, profile, window_feature, match_types))
      return *i;
  }
  return NULL;
}

Browser* FindBrowserWithTabbedOrAnyType(Profile* profile,
                                        chrome::HostDesktopType desktop_type,
                                        bool match_tabbed,
                                        bool match_original_profiles) {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (!browser_list_impl)
    return NULL;
  uint32_t match_types = kMatchAny;
  if (match_tabbed)
    match_types |= kMatchTabbed;
  if (match_original_profiles)
    match_types |= kMatchOriginalProfile;
  Browser* browser = FindBrowserMatching(browser_list_impl->begin_last_active(),
                                         browser_list_impl->end_last_active(),
                                         profile,
                                         Browser::FEATURE_NONE,
                                         match_types);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser : FindBrowserMatching(browser_list_impl->begin(),
                                                 browser_list_impl->end(),
                                                 profile,
                                                 Browser::FEATURE_NONE,
                                                 match_types);
}

size_t GetBrowserCountImpl(Profile* profile,
                           chrome::HostDesktopType desktop_type,
                           uint32_t match_types) {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  size_t count = 0;
  if (browser_list_impl) {
    for (BrowserList::const_iterator i = browser_list_impl->begin();
         i != browser_list_impl->end(); ++i) {
      if (BrowserMatches(*i, profile, Browser::FEATURE_NONE, match_types))
        count++;
    }
  }
  return count;
}

}  // namespace

namespace chrome {

Browser* FindTabbedBrowser(Profile* profile,
                           bool match_original_profiles,
                           HostDesktopType type) {
  return FindBrowserWithTabbedOrAnyType(profile,
                                        type,
                                        true,
                                        match_original_profiles);
}

Browser* FindAnyBrowser(Profile* profile,
                        bool match_original_profiles,
                        HostDesktopType type) {
  return FindBrowserWithTabbedOrAnyType(profile,
                                        type,
                                        false,
                                        match_original_profiles);
}

Browser* FindBrowserWithProfile(Profile* profile,
                                HostDesktopType desktop_type) {
  return FindBrowserWithTabbedOrAnyType(profile, desktop_type, false, false);
}

Browser* FindBrowserWithID(SessionID::id_type desired_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->session_id().id() == desired_id)
      return browser;
  }
  return NULL;
}

Browser* FindBrowserWithWindow(gfx::NativeWindow window) {
  if (!window)
    return NULL;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->window() && browser->window()->GetNativeWindow() == window)
      return browser;
  }
  return NULL;
}

Browser* FindBrowserWithWebContents(const WebContents* web_contents) {
  DCHECK(web_contents);
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (*it == web_contents)
      return it.browser();
  }
  return NULL;
}

Browser* FindLastActiveWithProfile(Profile* profile, HostDesktopType type) {
  BrowserList* list = BrowserList::GetInstance();
  // We are only interested in last active browsers, so we don't fall back to
  // all browsers like FindBrowserWith* do.
  return FindBrowserMatching(list->begin_last_active(), list->end_last_active(),
                             profile, Browser::FEATURE_NONE, kMatchAny);
}

Browser* FindLastActiveWithHostDesktopType(HostDesktopType type) {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (browser_list_impl)
    return browser_list_impl->GetLastActive();
  return NULL;
}

size_t GetTotalBrowserCount() {
  return BrowserList::GetInstance()->size();
}

size_t GetTotalBrowserCountForProfile(Profile* profile) {
  size_t count = 0;
  for (HostDesktopType t = HOST_DESKTOP_TYPE_FIRST; t < HOST_DESKTOP_TYPE_COUNT;
       t = static_cast<HostDesktopType>(t + 1)) {
    count += GetBrowserCount(profile, t);
  }
  return count;
}

size_t GetBrowserCount(Profile* profile, HostDesktopType type) {
  return GetBrowserCountImpl(profile, type, kMatchAny);
}

size_t GetTabbedBrowserCount(Profile* profile, HostDesktopType type) {
  return GetBrowserCountImpl(profile, type, kMatchTabbed);
}

}  // namespace chrome
