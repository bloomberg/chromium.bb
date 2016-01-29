// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_

#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
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
  class Observer {
   public:
    virtual ~Observer();
    virtual void OnBubbleAdded();
  };

  enum AutoResponseType {
    NONE,
    ACCEPT_ALL,
    DENY_ALL,
    DISMISS
  };

  ~PermissionBubbleManager() override;

  // Adds a new request to the permission bubble. Ownership of the request
  // remains with the caller. The caller must arrange for the request to
  // outlive the PermissionBubbleManager. If a bubble is visible when this
  // call is made, the request will be queued up and shown after the current
  // bubble closes. A request with message text identical to an outstanding
  // request will be merged with the outstanding request, and will have the same
  // callbacks called as the outstanding request.
  void AddRequest(PermissionBubbleRequest* request);

  // Cancels an outstanding request. This may have different effects depending
  // on what is going on with the bubble. If the request is pending, it will be
  // removed and never shown. If the request is showing, it will continue to be
  // shown, but the user's action won't be reported back to the request object.
  // In some circumstances, we can remove the request from the bubble, and may
  // do so. The request will have RequestFinished executed on it if it is found,
  // at which time the caller is free to delete the request.
  void CancelRequest(PermissionBubbleRequest* request);

  // Hides the bubble.
  void HideBubble();

  // Will show a permission bubble if there is a pending permission request on
  // the web contents that the PermissionBubbleManager belongs to.
  void DisplayPendingRequests();

  // Will reposition the bubble (may change parent if necessary).
  void UpdateAnchorPosition();

  // True if a permission bubble is currently visible.
  // TODO(hcarmona): Remove this as part of the bubble API work.
  bool IsBubbleVisible();

  // Get the native window of the bubble.
  // TODO(hcarmona): Remove this as part of the bubble API work.
  gfx::NativeWindow GetBubbleWindow();

  // For observing the status of the permission bubble manager.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Do NOT use this methods in production code. Use this methods in browser
  // tests that need to accept or deny permissions when requested in
  // JavaScript. Your test needs to set this appropriately, and then the bubble
  // will proceed as desired as soon as Show() is called.
  void set_auto_response_for_test(AutoResponseType response) {
    auto_response_for_test_ = response;
  }

 private:
  // TODO(felt): Update testing so that it doesn't involve a lot of friends.
  friend class DownloadRequestLimiterTest;
  friend class GeolocationBrowserTest;
  friend class GeolocationPermissionContextTests;
  friend class MockPermissionBubbleView;
  friend class PermissionBubbleManagerTest;
  friend class PermissionContextBaseTests;
  friend class content::WebContentsUserData<PermissionBubbleManager>;
  FRIEND_TEST_ALL_PREFIXES(DownloadTest, TestMultipleDownloadsBubble);

  explicit PermissionBubbleManager(content::WebContents* web_contents);

  // WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // PermissionBubbleView::Delegate:
  void ToggleAccept(int request_index, bool new_value) override;
  void Accept() override;
  void Deny() override;
  void Closing() override;

  // Posts a task which will allow the bubble to become visible if it is needed.
  void ScheduleShowBubble();

  // Shows the bubble if it is not already visible and there are pending
  // requests.
  void TriggerShowBubble();

  // Finalize the pending permissions request.
  void FinalizeBubble();

  // Cancel any pending requests. This is called if the WebContents
  // on which permissions calls are pending is destroyed or navigated away
  // from the requesting page.
  void CancelPendingQueues();

  // Searches |requests_|, |queued_requests_| and |queued_frame_requests_| - but
  // *not* |duplicate_requests_| - for a request matching |request|, and returns
  // the matching request, or |nullptr| if no match. Note that the matching
  // request may or may not be the same object as |request|.
  PermissionBubbleRequest* GetExistingRequest(PermissionBubbleRequest* request);

  // Returns true if |queue| contains a request which was generated by a user
  // gesture.  Returns false otherwise.
  bool HasUserGestureRequest(
      const std::vector<PermissionBubbleRequest*>& queue);

  // Calls PermissionGranted on a request and all its duplicates.
  void PermissionGrantedIncludingDuplicates(PermissionBubbleRequest* request);
  // Calls PermissionDenied on a request and all its duplicates.
  void PermissionDeniedIncludingDuplicates(PermissionBubbleRequest* request);
  // Calls Cancelled on a request and all its duplicates.
  void CancelledIncludingDuplicates(PermissionBubbleRequest* request);
  // Calls RequestFinished on a request and all its duplicates.
  void RequestFinishedIncludingDuplicates(PermissionBubbleRequest* request);

  void NotifyBubbleAdded();

  void DoAutoResponseForTesting();

  // Factory to be used to create views when needed.
  PermissionBubbleView::Factory view_factory_;

  // The UI surface to be used to display the permissions requests.
  scoped_ptr<PermissionBubbleView> view_;

  std::vector<PermissionBubbleRequest*> requests_;
  std::vector<PermissionBubbleRequest*> queued_requests_;
  std::vector<PermissionBubbleRequest*> queued_frame_requests_;
  // Maps from the first request of a kind to subsequent requests that were
  // duped against it.
  std::unordered_multimap<PermissionBubbleRequest*, PermissionBubbleRequest*>
      duplicate_requests_;

  // URL of the main frame in the WebContents to which this manager is attached.
  // TODO(gbillock): if there are iframes in the page, we need to deal with it.
  GURL request_url_;
  bool main_frame_has_fully_loaded_;

  // Whether each of the requests in |requests_| is accepted by the user.
  std::vector<bool> accept_states_;

  base::ObserverList<Observer> observer_list_;
  AutoResponseType auto_response_for_test_;

  base::WeakPtrFactory<PermissionBubbleManager> weak_factory_;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_BUBBLE_MANAGER_H_
