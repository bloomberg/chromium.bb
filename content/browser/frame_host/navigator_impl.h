// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/tuple.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

class GURL;
struct FrameMsg_Navigate_Params;

namespace content {

class NavigationControllerImpl;
class NavigatorDelegate;
struct LoadCommittedDetails;
struct CommitNavigationParams;
struct CommonNavigationParams;
struct RequestNavigationParams;

// This class is an implementation of Navigator, responsible for managing
// navigations in regular browser tabs.
class CONTENT_EXPORT NavigatorImpl : public Navigator {
 public:
  NavigatorImpl(NavigationControllerImpl* navigation_controller,
                NavigatorDelegate* delegate);

  // Navigator implementation.
  virtual NavigationController* GetController() OVERRIDE;
  virtual void DidStartProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                                       const GURL& url,
                                       bool is_transition_navigation) OVERRIDE;
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params)
      OVERRIDE;
  virtual void DidFailLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      int error_code,
      const base::string16& error_description) OVERRIDE;
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params&
          input_params) OVERRIDE;
  virtual bool NavigateToPendingEntry(
      RenderFrameHostImpl* render_frame_host,
      NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                              const GURL& url,
                              const Referrer& referrer,
                              WindowOpenDisposition disposition,
                              bool should_replace_current_entry,
                              bool user_gesture) OVERRIDE;
  virtual void RequestTransferURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const std::vector<GURL>& redirect_chain,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      WindowOpenDisposition disposition,
      const GlobalRequestID& transferred_global_request_id,
      bool should_replace_current_entry,
      bool user_gesture) OVERRIDE;
  virtual void CommitNavigation(
      RenderFrameHostImpl* render_frame_host,
      const GURL& stream_url,
      const CommonNavigationParams& common_params,
      const CommitNavigationParams& commit_params) OVERRIDE;
  virtual void LogResourceRequestTime(
      base::TimeTicks timestamp, const GURL& url) OVERRIDE;

 private:
  virtual ~NavigatorImpl();

  // Navigates to the given entry, which must be the pending entry.  Private
  // because all callers should use NavigateToPendingEntry.
  bool NavigateToEntry(
      RenderFrameHostImpl* render_frame_host,
      const NavigationEntryImpl& entry,
      NavigationController::ReloadType reload_type);

  bool ShouldAssignSiteForURL(const GURL& url);

  void CheckWebUIRendererDoesNotDisplayNormalURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url);

  // The NavigationController that will keep track of session history for all
  // RenderFrameHost objects using this NavigatorImpl.
  // TODO(nasko): Move ownership of the NavigationController from
  // WebContentsImpl to this class.
  NavigationControllerImpl* controller_;

  // Used to notify the object embedding this Navigator about navigation
  // events. Can be NULL in tests.
  NavigatorDelegate* delegate_;

  // The start time and URL for latest navigation request, used for feeding a
  // few histograms under the Navigation group.
  Tuple2<base::TimeTicks, GURL> navigation_start_time_and_url;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
