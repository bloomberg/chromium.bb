// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_

#include <map>

#include "base/id_map.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace chrome {
struct NavigateParams;
}

namespace blink {
struct WebWindowFeatures;
}

class GURL;

// Per-tab class to manage blocked popups.
class PopupBlockerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PopupBlockerTabHelper> {
 public:
  // Mapping from popup IDs to blocked popup requests.
  typedef std::map<int32, GURL> PopupIdMap;

  virtual ~PopupBlockerTabHelper();

  // Returns true if the popup request defined by |params| should be blocked.
  // In that case, it is also added to the |blocked_popups_| container.
  bool MaybeBlockPopup(const chrome::NavigateParams& params,
                       const blink::WebWindowFeatures& window_features);

  // Adds a popup request to the |blocked_popups_| container.
  void AddBlockedPopup(const BlockedWindowParams& params);

  // Creates the blocked popup with |popup_id|.
  void ShowBlockedPopup(int32 popup_id);

  // Returns the number of blocked popups.
  size_t GetBlockedPopupsCount() const;

  PopupIdMap GetBlockedPopupRequests();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  struct BlockedRequest;
  friend class content::WebContentsUserData<PopupBlockerTabHelper>;

  explicit PopupBlockerTabHelper(content::WebContents* web_contents);

  // Called when the blocked popup notification is shown or hidden.
  void PopupNotificationVisibilityChanged(bool visible);

  IDMap<BlockedRequest, IDMapOwnPointer> blocked_popups_;

  DISALLOW_COPY_AND_ASSIGN(PopupBlockerTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_TAB_HELPER_H_
