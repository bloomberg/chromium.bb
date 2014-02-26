// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_

#include <vector>

#include "base/timer/timer.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PermissionBubbleRequest;

// Provides access to permissions bubbles. Allows clients to add a request
// callback interface to the existing permission bubble configuration.
// Depending on the situation and policy, that may add new UI to an existing
// permission bubble, create and show a new permission bubble, or provide no
// visible UI action at all. (In that case, the request will be immediately
// informed that the permission request failed.)
//
// A PermissionBubbleManager is associated with a particular WebContents.
// Requests attached to a particular WebContents' PBM must outlive it.
//
// The PermissionBubbleManager should be addressed on the UI thread.
class PermissionBubbleManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PermissionBubbleManager>,
      public PermissionBubbleView::Delegate {
 public:
  // Return the flag-driven enabled state of permissions bubbles.
  static bool Enabled();

  virtual ~PermissionBubbleManager();

  // Add a new request to the permission bubble. Ownership of the request
  // remains with the caller. The caller must arrange for the request to
  // outlive the PermissionBubbleManager. If a bubble is visible when this
  // call is made, the request will be queued up and shown after the current
  // bubble closes.
  virtual void AddRequest(PermissionBubbleRequest* request);

  // Set the active view for the permission bubble. If this is NULL, it
  // means the permission bubble is no longer showing.
  virtual void SetView(PermissionBubbleView* view) OVERRIDE;

 protected:
  // Sets the coalesce time interval to |interval_ms|. For testing only.
  void SetCoalesceIntervalForTesting(int interval_ms);

 private:
  friend class PermissionBubbleManagerTest;
  friend class DownloadRequestLimiterTest;
  friend class content::WebContentsUserData<PermissionBubbleManager>;

  explicit PermissionBubbleManager(content::WebContents* web_contents);

  // contents::WebContentsObserver:
  // TODO(leng):  Investigate the ordering and timing of page loading and
  // permission requests with iFrames. DocumentOnLoadCompletedInMainFrame()
  // and DocumentLoadedInFrame() might be needed as well.
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  // PermissionBubbleView::Delegate:
  virtual void ToggleAccept(int request_index, bool new_value) OVERRIDE;
  virtual void SetCustomizationMode() OVERRIDE;
  virtual void Accept() OVERRIDE;
  virtual void Deny() OVERRIDE;
  virtual void Closing() OVERRIDE;

  // Called when the coalescing timer is done. Presents the bubble.
  void ShowBubble();

  // Finalize the pending permissions request.
  void FinalizeBubble();

  // Whether or not we are showing the bubble in this tab.
  bool bubble_showing_;

  // Set to the UI surface to be used to display the permissions requests.
  PermissionBubbleView* view_;

  std::vector<PermissionBubbleRequest*> requests_;
  std::vector<PermissionBubbleRequest*> queued_requests_;
  std::vector<bool> accept_states_;
  bool customization_mode_;

  scoped_ptr<base::Timer> timer_;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
