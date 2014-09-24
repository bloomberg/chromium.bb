// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/window_open_disposition.h"

class GURL;
struct FrameHostMsg_BeginNavigation_Params;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;

namespace base {
class TimeTicks;
}

namespace content {

class NavigationControllerImpl;
class NavigationEntryImpl;
class NavigatorDelegate;
class RenderFrameHostImpl;
struct NavigationBeforeCommitInfo;

// Implementations of this interface are responsible for performing navigations
// in a node of the FrameTree. Its lifetime is bound to all FrameTreeNode
// objects that are using it and will be released once all nodes that use it are
// freed. The Navigator is bound to a single frame tree and cannot be used by
// multiple instances of FrameTree.
// TODO(nasko): Move all navigation methods, such as didStartProvisionalLoad
// from WebContentsImpl to this interface.
class CONTENT_EXPORT Navigator : public base::RefCounted<Navigator> {
 public:
  // Returns the NavigationController associated with this Navigator.
  virtual NavigationController* GetController();

  // Notifications coming from the RenderFrameHosts ----------------------------

  // The RenderFrameHostImpl started a provisional load.
  virtual void DidStartProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                                       const GURL& url,
                                       bool is_transition_navigation) {};

  // The RenderFrameHostImpl has failed a provisional load.
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {};

  // The RenderFrameHostImpl has failed to load the document.
  virtual void DidFailLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      int error_code,
      const base::string16& error_description) {}

  // The RenderFrameHostImpl has committed a navigation.
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {}

  // Called by the NavigationController to cause the Navigator to navigate
  // to the current pending entry. The NavigationController should be called
  // back with RendererDidNavigate on success or DiscardPendingEntry on failure.
  // The callbacks can be inside of this function, or at some future time.
  //
  // The entry has a PageID of -1 if newly created (corresponding to navigation
  // to a new URL).
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  //
  // TODO(nasko): Remove this method from the interface, since Navigator and
  // NavigationController know about each other. This will be possible once
  // initialization of Navigator and NavigationController is properly done.
  virtual bool NavigateToPendingEntry(
      RenderFrameHostImpl* render_frame_host,
      NavigationController::ReloadType reload_type);


  // Navigation requests -------------------------------------------------------

  virtual base::TimeTicks GetCurrentLoadStart();

  // The RenderFrameHostImpl has received a request to open a URL with the
  // specified |disposition|.
  virtual void RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                              const GURL& url,
                              const Referrer& referrer,
                              WindowOpenDisposition disposition,
                              bool should_replace_current_entry,
                              bool user_gesture) {}

  // The RenderFrameHostImpl wants to transfer the request to a new renderer.
  // |redirect_chain| contains any redirect URLs (excluding |url|) that happened
  // before the transfer.
  virtual void RequestTransferURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const std::vector<GURL>& redirect_chain,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      WindowOpenDisposition disposition,
      const GlobalRequestID& transferred_global_request_id,
      bool should_replace_current_entry,
      bool user_gesture) {}

  // PlzNavigate
  // Signal |render_frame_host| that a navigation is ready to commit (the
  // response to the navigation request has been received).
  virtual void CommitNavigation(RenderFrameHostImpl* render_frame_host,
                                const NavigationBeforeCommitInfo& info) {};

 protected:
  friend class base::RefCounted<Navigator>;
  virtual ~Navigator() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
