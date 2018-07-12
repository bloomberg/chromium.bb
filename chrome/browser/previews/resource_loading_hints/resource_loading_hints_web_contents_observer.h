// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Observes navigation events and sends the resource loading hints mojo message
// to the renderer.
class ResourceLoadingHintsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          ResourceLoadingHintsWebContentsObserver> {
 public:
  ~ResourceLoadingHintsWebContentsObserver() override;

 private:
  friend class content::WebContentsUserData<
      ResourceLoadingHintsWebContentsObserver>;

  explicit ResourceLoadingHintsWebContentsObserver(
      content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver. If the navigation is of type
  // resource loading hints preview, then this method sends the resource loading
  // hints mojo message to the renderer.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Sends resource loading hints to the renderer.
  void SendResourceLoadingHints(
      content::NavigationHandle* navigation_handle) const;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsWebContentsObserver);
};

#endif  // CHROME_BROWSER_PREVIEWS_RESOURCE_LOADING_HINTS_RESOURCE_LOADING_HINTS_WEB_CONTENTS_OBSERVER_H_
