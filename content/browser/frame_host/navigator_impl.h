// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/common/previews_state.h"
#include "url/gurl.h"

class GURL;

namespace content {

class NavigationControllerImpl;
class NavigatorDelegate;
struct LoadCommittedDetails;

// This class is an implementation of Navigator, responsible for managing
// navigations in regular browser tabs.
class CONTENT_EXPORT NavigatorImpl : public Navigator {
 public:
  NavigatorImpl(NavigationControllerImpl* navigation_controller,
                NavigatorDelegate* delegate);

  static void CheckWebUIRendererDoesNotDisplayNormalURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url);

  // Navigator implementation.
  NavigatorDelegate* GetDelegate() override;
  NavigationController* GetController() override;
  void DidStartProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const std::vector<GURL>& redirect_chain,
      const base::TimeTicks& navigation_start) override;
  void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params)
      override;
  void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                            const GURL& url,
                            int error_code,
                            const base::string16& error_description) override;
  void DidNavigate(RenderFrameHostImpl* render_frame_host,
                   const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
                   std::unique_ptr<NavigationHandleImpl> navigation_handle,
                   bool was_within_same_document) override;
  bool NavigateToPendingEntry(
      FrameTreeNode* frame_tree_node,
      const FrameNavigationEntry& frame_entry,
      ReloadType reload_type,
      bool is_same_document_history_load,
      std::unique_ptr<NavigationUIData> navigation_ui_data) override;
  bool NavigateNewChildFrame(RenderFrameHostImpl* render_frame_host,
                             const GURL& default_url) override;
  void RequestOpenURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      bool uses_post,
      const scoped_refptr<network::ResourceRequestBody>& body,
      const std::string& extra_headers,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      bool should_replace_current_entry,
      bool user_gesture,
      blink::WebTriggeringEventInfo triggering_event_info,
      const base::Optional<std::string>& suggested_filename) override;
  void RequestTransferURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      SiteInstance* source_site_instance,
      const std::vector<GURL>& redirect_chain,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      const GlobalRequestID& transferred_global_request_id,
      bool should_replace_current_entry,
      const std::string& method,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const std::string& extra_headers,
      const base::Optional<std::string>& suggested_filename) override;
  void OnBeforeUnloadACK(FrameTreeNode* frame_tree_node,
                         bool proceed,
                         const base::TimeTicks& proceed_time) override;
  void OnBeginNavigation(FrameTreeNode* frame_tree_node,
                         const CommonNavigationParams& common_params,
                         mojom::BeginNavigationParamsPtr begin_params) override;
  void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request) override;
  void OnAbortNavigation(FrameTreeNode* frame_tree_node) override;
  void LogResourceRequestTime(base::TimeTicks timestamp,
                              const GURL& url) override;
  void LogBeforeUnloadTime(
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time) override;
  void CancelNavigation(FrameTreeNode* frame_tree_node,
                        bool inform_renderer) override;
  void DiscardPendingEntryIfNeeded(int expected_pending_entry_id) override;

 private:
  // Holds data used to track browser side navigation metrics.
  struct NavigationMetricsData;

  friend class NavigatorTestWithBrowserSideNavigation;
  ~NavigatorImpl() override;

  // Navigates to the given entry, which might be the pending entry (if
  // |is_pending_entry| is true).  Private because all callers should use either
  // NavigateToPendingEntry or NavigateToNewChildFrame.
  bool NavigateToEntry(
      FrameTreeNode* frame_tree_node,
      const FrameNavigationEntry& frame_entry,
      const NavigationEntryImpl& entry,
      ReloadType reload_type,
      bool is_same_document_history_load,
      bool is_history_navigation_in_new_child,
      bool is_pending_entry,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      std::unique_ptr<NavigationUIData> navigation_ui_data);

  // If needed, sends a BeforeUnload IPC to the renderer to ask it to execute
  // the beforeUnload event. Otherwise, the navigation request will be started.
  void RequestNavigation(
      FrameTreeNode* frame_tree_node,
      const GURL& dest_url,
      const Referrer& dest_referrer,
      const FrameNavigationEntry& frame_entry,
      const NavigationEntryImpl& entry,
      ReloadType reload_type,
      PreviewsState previews_state,
      bool is_same_document_history_load,
      bool is_history_navigation_in_new_child,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      base::TimeTicks navigation_start,
      std::unique_ptr<NavigationUIData> navigation_ui_data);

  void RecordNavigationMetrics(
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      SiteInstance* site_instance);

  // Called when a navigation has started in a main frame, to update the pending
  // NavigationEntry if the controller does not currently have a
  // browser-initiated one.
  void DidStartMainFrameNavigation(const GURL& url,
                                   SiteInstanceImpl* site_instance,
                                   NavigationHandleImpl* navigation_handle);

  // The NavigationController that will keep track of session history for all
  // RenderFrameHost objects using this NavigatorImpl.
  // TODO(nasko): Move ownership of the NavigationController from
  // WebContentsImpl to this class.
  NavigationControllerImpl* controller_;

  // Used to notify the object embedding this Navigator about navigation
  // events. Can be NULL in tests.
  NavigatorDelegate* delegate_;

  std::unique_ptr<NavigatorImpl::NavigationMetricsData> navigation_data_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
