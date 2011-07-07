// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_DETAILS_H_
#define CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_DETAILS_H_
#pragma once

#include <string>
#include "content/common/navigation_types.h"
#include "googleurl/src/gurl.h"

class NavigationEntry;

namespace content {

// Provides the details for a NOTIFY_NAV_ENTRY_COMMITTED notification.
// TODO(brettw) this mostly duplicates ProvisionalLoadDetails, it would be
// nice to unify these somehow.
struct LoadCommittedDetails {
  // By default, the entry will be filled according to a new main frame
  // navigation.
  LoadCommittedDetails();

  // The committed entry. This will be the active entry in the controller.
  NavigationEntry* entry;

  // The type of navigation that just occurred. Note that not all types of
  // navigations in the enum are valid here, since some of them don't actually
  // cause a "commit" and won't generate this notification.
  NavigationType::Type type;

  // The index of the previously committed navigation entry. This will be -1
  // if there are no previous entries.
  int previous_entry_index;

  // The previous URL that the user was on. This may be empty if none.
  GURL previous_url;

  // True when this load was non-user initated. This corresponds to a
  // a NavigationGestureAuto call from WebKit (see webview_delegate.h).
  // We also count reloads and meta-refreshes as "auto" to account for the
  // fact that WebKit doesn't always set the user gesture properly in these
  // cases (see bug 1051891).
  bool is_auto;

  // True if the committed entry has replaced the exisiting one.
  // A non-user initiated redirect causes such replacement.
  // This is somewhat similiar to is_auto, but not exactly the same.
  bool did_replace_entry;

  // True if the navigation was in-page. This means that the active entry's
  // URL and the |previous_url| are the same except for reference fragments.
  bool is_in_page;

  // True when the main frame was navigated. False means the navigation was a
  // sub-frame.
  bool is_main_frame;

  // When the committed load is a web page from the renderer, this string
  // specifies the security state if the page is secure.
  // See ViewHostMsg_FrameNavigate_Params.security_info, where it comes from.
  // Use SSLManager::DeserializeSecurityInfo to decode it.
  std::string serialized_security_info;

  // Returns whether the user probably felt like they navigated somewhere new.
  // We often need this logic for showing or hiding something, and this
  // returns true only for main frame loads that the user initiated, that go
  // to a new page.
  bool is_user_initiated_main_frame_load() const {
    return !is_auto && !is_in_page && is_main_frame;
  }

  // The HTTP status code for this entry..
  int http_status_code;
};

// Provides the details for a NOTIFY_NAV_ENTRY_CHANGED notification.
struct EntryChangedDetails {
  // The changed navigation entry after it has been updated.
  const NavigationEntry* changed_entry;

  // Indicates the current index in the back/forward list of the entry.
  int index;
};

// Details sent for NOTIFY_NAV_LIST_PRUNED.
struct PrunedDetails {
  // If true, count items were removed from the front of the list, otherwise
  // count items were removed from the back of the list.
  bool from_front;

  // Number of items removed.
  int count;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_DETAILS_H_
