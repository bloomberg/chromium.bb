// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/id_map.h"
#include "base/macros.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace chrome {
struct NavigateParams;
}

namespace content {
struct OpenURLParams;
}

class GURL;

// Per-tab class to manage blocked popups.
class PopupBlockerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PopupBlockerTabHelper> {
 public:
  // Mapping from popup IDs to blocked popup requests.
  typedef std::map<int32_t, GURL> PopupIdMap;

  // Returns true if a popup with |user_gesture| should be considered for
  // blocking from |web_contents|.
  static bool ConsiderForPopupBlocking(
      content::WebContents* web_contents,
      bool user_gesture,
      const content::OpenURLParams* open_url_params);

  ~PopupBlockerTabHelper() override;

  // Returns true if the popup request defined by |params| should be blocked.
  // In that case, it is also added to the |blocked_popups_| container.
  bool MaybeBlockPopup(const chrome::NavigateParams& params,
                       const blink::mojom::WindowFeatures& window_features);

  // Adds a popup request to the |blocked_popups_| container.
  void AddBlockedPopup(const BlockedWindowParams& params);

  // Creates the blocked popup with |popup_id|.
  void ShowBlockedPopup(int32_t popup_id);

  // Returns the number of blocked popups.
  size_t GetBlockedPopupsCount() const;

  PopupIdMap GetBlockedPopupRequests();

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  struct BlockedRequest;
  friend class content::WebContentsUserData<PopupBlockerTabHelper>;

  explicit PopupBlockerTabHelper(content::WebContents* web_contents);

  void AddBlockedPopup(const chrome::NavigateParams& params,
                       const blink::mojom::WindowFeatures& window_features);

  // Called when the blocked popup notification is shown or hidden.
  void PopupNotificationVisibilityChanged(bool visible);

  IDMap<std::unique_ptr<BlockedRequest>> blocked_popups_;

  DISALLOW_COPY_AND_ASSIGN(PopupBlockerTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
