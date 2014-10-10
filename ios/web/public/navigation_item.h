// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_ITEM_H_
#define IOS_WEB_PUBLIC_NAVIGATION_ITEM_H_

#include "base/strings/string16.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace web {

// A NavigationItem is a data structure that captures all the information
// required to recreate a browsing state. It represents one point in the
// chain of navigation managed by a NavigationManager.
class NavigationItem {
 public:
  virtual ~NavigationItem() {}

  // The actual URL of the page. For some about pages, this may be a scary
  // data: URL or something like that. Use GetVirtualURL() below for showing to
  // the user.
  virtual const GURL& GetURL() const = 0;

  // The URL that should be shown to the user. In most cases this is the same
  // as the URL above, but in some case the underlying URL might not be
  // suitable for display to the user.
  virtual const GURL& GetVirtualURL() const = 0;

  // The title as set by the page. This will be empty if there is no title set.
  // The caller is responsible for detecting when there is no title and
  // displaying the appropriate "Untitled" label if this is being displayed to
  // the user.
  virtual const base::string16& GetTitle() const = 0;

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  virtual ui::PageTransition GetTransitionType() const = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_ITEM_H_
