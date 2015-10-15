// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_NAVIGATION_ENTRY_H_
#define COMPONENTS_WEB_VIEW_NAVIGATION_ENTRY_H_

#include "components/web_view/url_request_cloneable.h"

#include "url/gurl.h"

namespace web_view {

// Contains all information needed about an individual navigation in the
// navigation stack.
class NavigationEntry {
 public:
  explicit NavigationEntry(mojo::URLRequestPtr original_request);
  explicit NavigationEntry(const GURL& raw_url);
  ~NavigationEntry();

  // Builds a copy of the URLRequest that generated this navigation. This
  // method is heavyweight as it clones a few mojo pipes.
  mojo::URLRequestPtr BuildURLRequest(bool update_originating_time);

 private:
  // TODO(erg): This is not enough information to regenerate the state of the
  // world. This is only enough information to regenerate some top level frame
  // navigations. A full implementation would require individual
  // FrameNavigationEntry objects like in content::NavigationEntryImpl.
  URLRequestCloneable url_request_;

  DISALLOW_COPY_AND_ASSIGN(NavigationEntry);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_NAVIGATION_ENTRY_H_
