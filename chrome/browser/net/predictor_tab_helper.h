// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PREDICTOR_TAB_HELPER_H_
#define CHROME_BROWSER_NET_PREDICTOR_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

namespace chrome_browser_net {

class PredictorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PredictorTabHelper> {
 public:
  ~PredictorTabHelper() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::ReloadType reload_type) override;
  void DocumentOnLoadCompletedInMainFrame() override;

 private:
  explicit PredictorTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PredictorTabHelper>;

  void PreconnectUrl(const GURL& url);

  // This boolean is set to true after a call to
  // DidStartNavigationToPendingEntry, which fires off predictive preconnects.
  // This ensures that the resulting call to DidStartNavigation does not
  // duplicate these preconnects. The tab helper spawns preconnects on the
  // navigation to the pending entry because DidStartNavigation is called after
  // render process spawn (pre-PlzNavigate), which can take substantial time
  // especially on Android.
  bool predicted_from_pending_entry_;

  DISALLOW_COPY_AND_ASSIGN(PredictorTabHelper);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PREDICTOR_TAB_HELPER_H_
