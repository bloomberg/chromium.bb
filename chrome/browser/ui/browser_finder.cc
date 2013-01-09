// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_impl.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"

using content::WebContents;

namespace {

// TODO(mad) eventually move this to host_desktop_type.h.
#if defined(OS_CHROMEOS)
chrome::HostDesktopType kDefaultHostDesktopType = chrome::HOST_DESKTOP_TYPE_ASH;
#else
chrome::HostDesktopType kDefaultHostDesktopType =
    chrome::HOST_DESKTOP_TYPE_NATIVE;
#endif


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
                                        chrome::HostDesktopType desktop_type,
                                        bool match_tabbed,
                                        bool match_original_profiles) {
  chrome::BrowserListImpl* browser_list_impl =
      chrome::BrowserListImpl::GetInstance(desktop_type);
  if (!browser_list_impl)
    return NULL;
  uint32 match_types = kMatchAny;
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
                           uint32 match_types) {
  chrome::BrowserListImpl* browser_list_impl =
      chrome::BrowserListImpl::GetInstance(desktop_type);
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

Browser* FindOrCreateTabbedBrowser(Profile* profile, HostDesktopType type) {
  Browser* browser = FindTabbedBrowser(profile, false, type);
  if (!browser)
    browser = new Browser(Browser::CreateParams(profile, type));
  return browser;
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
    if (browser->window() && browser->window()->GetNativeWindow() == window)
      return browser;
  }
  return NULL;
}

Browser* FindBrowserWithWebContents(const WebContents* web_contents) {
  DCHECK(web_contents);
  for (TabContentsIterator it; !it.done(); ++it) {
    if (*it == web_contents)
      return it.browser();
  }
  return NULL;
}

HostDesktopType FindHostDesktopTypeForWebContents(
    const WebContents* web_contents) {
  Browser* browser = FindBrowserWithWebContents(web_contents);
  return browser ? browser->host_desktop_type() : HOST_DESKTOP_TYPE_NATIVE;
}

Browser* FindLastActiveWithProfile(Profile* profile, HostDesktopType type) {
  BrowserListImpl* list = BrowserListImpl::GetInstance(type);
  // We are only interested in last active browsers, so we don't fall back to
  // all browsers like FindBrowserWith* do.
  return FindBrowserMatching(list->begin_last_active(), list->end_last_active(),
                             profile, Browser::FEATURE_NONE, kMatchAny);
}

Browser* FindLastActiveWithHostDesktopType(HostDesktopType type) {
  BrowserListImpl* browser_list_impl = BrowserListImpl::GetInstance(type);
  if (browser_list_impl)
    return browser_list_impl->GetLastActive();
  return NULL;
}

size_t GetBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kDefaultHostDesktopType, kMatchAny);
}

size_t GetTabbedBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kDefaultHostDesktopType, kMatchTabbed);
}

}  // namespace chrome
