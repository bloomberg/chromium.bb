// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"

class GURL;

namespace content {

// A NavigationEntry is a data structure that captures all the information
// required to recreate a browsing state. This includes some opaque binary
// state as provided by the TabContents as well as some clear text title and
// URL which is used for our user interface.
class NavigationEntry {
 public:
  virtual ~NavigationEntry() {}

  // Page-related stuff --------------------------------------------------------

  // A unique ID is preserved across commits and redirects, which means that
  // sometimes a NavigationEntry's unique ID needs to be set (e.g. when
  // creating a committed entry to correspond to a to-be-deleted pending entry,
  // the pending entry's ID must be copied).
  virtual int GetUniqueID() const = 0;

  // The actual URL of the page. For some about pages, this may be a scary
  // data: URL or something like that. Use GetVirtualURL() below for showing to
  // the user.
  virtual const GURL& GetURL() const = 0;

  // The referring URL. Can be empty.
  virtual const content::Referrer& GetReferrer() const = 0;

  // The virtual URL, when nonempty, will override the actual URL of the page
  // when we display it to the user. This allows us to have nice and friendly
  // URLs that the user sees for things like about: URLs, but actually feed
  // the renderer a data URL that results in the content loading.
  //
  // GetVirtualURL() will return the URL to display to the user in all cases, so
  // if there is no overridden display URL, it will return the actual one.
  virtual void SetVirtualURL(const GURL& url) = 0;
  virtual const GURL& GetVirtualURL() const = 0;

  // The title as set by the page. This will be empty if there is no title set.
  // The caller is responsible for detecting when there is no title and
  // displaying the appropriate "Untitled" label if this is being displayed to
  // the user.
  virtual void SetTitle(const string16& title) = 0;
  virtual const string16& GetTitle() const = 0;

  // Content state is an opaque blob created by WebKit that represents the
  // state of the page. This includes form entries and scroll position for each
  // frame. We store it so that we can supply it back to WebKit to restore form
  // state properly when the user goes back and forward.
  //
  // WARNING: This state is saved to the file and used to restore previous
  // states. If the format is modified in the future, we should still be able to
  // deal with older versions.
  virtual void SetContentState(const std::string& state) = 0;
  virtual const std::string& GetContentState() const = 0;

  // Describes the current page that the tab represents. For web pages
  // (TAB_CONTENTS_WEB) this is the ID that the renderer generated for the page
  // and is how we can tell new versus renavigations.
  virtual void SetPageID(int page_id) = 0;
  virtual int32 GetPageID() const = 0;

  // Page-related helpers ------------------------------------------------------

  // Returns the title to be displayed on the tab. This could be the title of
  // the page if it is available or the URL. |languages| is the list of
  // accpeted languages (e.g., prefs::kAcceptLanguages) or empty if proper
  // URL formatting isn't needed (e.g., unit tests).
  virtual const string16& GetTitleForDisplay(
      const std::string& languages) const = 0;

  // Returns true if the current tab is in view source mode. This will be false
  // if there is no navigation.
  virtual bool IsViewSourceMode() const = 0;

  // Tracking stuff ------------------------------------------------------------

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  virtual content::PageTransition GetTransitionType() const = 0;

  // Post data is form data that was posted to get to this page. The data will
  // have to be reposted to reload the page properly. This flag indicates
  // whether the page had post data.
  //
  // The actual post data is stored in the content_state and is extracted by
  // WebKit to actually make the request.
  virtual void SetHasPostData(bool has_post_data) = 0;
  virtual bool GetHasPostData() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_H_
