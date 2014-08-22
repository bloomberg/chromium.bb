// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/common/content_export.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/page_transition_types.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/accessibility/ax_node_data.h"

class GURL;
struct AccessibilityHostMsg_EventParams;
struct AccessibilityHostMsg_LocationChangeParams;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;
struct FrameHostMsg_OpenURL_Params;
struct FrameHostMsg_BeginNavigation_Params;
struct FrameMsg_Navigate_Params;

namespace base {
class FilePath;
class ListValue;
}

namespace content {

class CrossProcessFrameConnector;
class CrossSiteTransferringRequest;
class FrameTree;
class FrameTreeNode;
class RenderFrameHostDelegate;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostImpl;
struct ContextMenuParams;
struct GlobalRequestID;
struct Referrer;
struct ShowDesktopNotificationHostMsgParams;
struct TransitionLayerData;

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      public BrowserAccessibilityDelegate {
 public:
  static RenderFrameHostImpl* FromID(int process_id, int routing_id);

  virtual ~RenderFrameHostImpl();

  // RenderFrameHost
  virtual int GetRoutingID() OVERRIDE;
  virtual SiteInstance* GetSiteInstance() OVERRIDE;
  virtual RenderProcessHost* GetProcess() OVERRIDE;
  virtual RenderFrameHost* GetParent() OVERRIDE;
  virtual const std::string& GetFrameName() OVERRIDE;
  virtual bool IsCrossProcessSubframe() OVERRIDE;
  virtual GURL GetLastCommittedURL() OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual void ExecuteJavaScript(
      const base::string16& javascript) OVERRIDE;
  virtual void ExecuteJavaScript(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback) OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() OVERRIDE;
  virtual ServiceRegistry* GetServiceRegistry() OVERRIDE;

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // BrowserAccessibilityDelegate
  virtual void AccessibilitySetFocus(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityShowMenu(const gfx::Point& global_point) OVERRIDE;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, const gfx::Rect& subfocus) OVERRIDE;
  virtual void AccessibilityScrollToPoint(
      int acc_obj_id, const gfx::Point& point) OVERRIDE;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) OVERRIDE;
  virtual bool AccessibilityViewHasFocus() const OVERRIDE;
  virtual gfx::Rect AccessibilityGetViewBounds() const OVERRIDE;
  virtual gfx::Point AccessibilityOriginInScreen(const gfx::Rect& bounds)
      const OVERRIDE;
  virtual void AccessibilityHitTest(const gfx::Point& point) OVERRIDE;
  virtual void AccessibilityFatalError() OVERRIDE;
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() OVERRIDE;
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible()
      OVERRIDE;

  bool CreateRenderFrame(int parent_routing_id);
  bool IsRenderFrameLive();
  void Init();
  int routing_id() const { return routing_id_; }
  void OnCreateChildFrame(int new_routing_id,
                          const std::string& frame_name);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }
  // TODO(nasko): The RenderWidgetHost will be owned by RenderFrameHost in
  // the future, so update this accessor to return the right pointer.
  RenderWidgetHostImpl* GetRenderWidgetHost();

  // This function is called when this is a swapped out RenderFrameHost that
  // lives in the same process as the parent frame. The
  // |cross_process_frame_connector| allows the non-swapped-out
  // RenderFrameHost for a frame to communicate with the parent process
  // so that it may composite drawing data.
  //
  // Ownership is not transfered.
  void set_cross_process_frame_connector(
      CrossProcessFrameConnector* cross_process_frame_connector) {
    cross_process_frame_connector_ = cross_process_frame_connector;
  }

  void set_render_frame_proxy_host(RenderFrameProxyHost* proxy) {
    render_frame_proxy_host_ = proxy;
  }

  // Returns a bitwise OR of bindings types that have been enabled for this
  // RenderFrameHostImpl's RenderView. See BindingsPolicy for details.
  // TODO(creis): Make bindings frame-specific, to support cases like <webview>.
  int GetEnabledBindings();

  // Called on the pending RenderFrameHost when the network response is ready to
  // commit.  We should ensure that the old RenderFrameHost runs its unload
  // handler and determine whether a transfer to a different RenderFrameHost is
  // needed.
  void OnCrossSiteResponse(
      const GlobalRequestID& global_request_id,
      scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
      const std::vector<GURL>& transfer_url_chain,
      const Referrer& referrer,
      PageTransition page_transition,
      bool should_replace_current_entry);

  // Called on the current RenderFrameHost when the network response is first
  // receieved.
  void OnDeferredAfterResponseStarted(
      const GlobalRequestID& global_request_id,
      const TransitionLayerData& transition_data);

  // Tells the renderer that this RenderFrame is being swapped out for one in a
  // different renderer process.  It should run its unload handler, move to
  // a blank document and create a RenderFrameProxy to replace the RenderFrame.
  // The renderer should preserve the Proxy object until it exits, in case we
  // come back.  The renderer can exit if it has no other active RenderFrames,
  // but not until WasSwappedOut is called (when it is no longer visible).
  void SwapOut(RenderFrameProxyHost* proxy);

  void OnSwappedOut(bool timed_out);
  bool is_swapped_out() { return is_swapped_out_; }
  void set_swapped_out(bool is_swapped_out) {
    is_swapped_out_ = is_swapped_out;
  }

  // Sets the RVH for |this| as pending shutdown. |on_swap_out| will be called
  // when the SwapOutACK is received.
  void SetPendingShutdown(const base::Closure& on_swap_out);

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const FrameMsg_Navigate_Params& params);

  // Load the specified URL; this is a shortcut for Navigate().
  void NavigateToURL(const GURL& url);

  // Stop the load in progress.
  void Stop();

  // Returns whether navigation messages are currently suspended for this
  // RenderFrameHost. Only true during a cross-site navigation, while waiting
  // for the onbeforeunload handler.
  bool are_navigations_suspended() const { return navigations_suspended_; }

  // Suspends (or unsuspends) any navigation messages from being sent from this
  // RenderFrameHost. This is called when a pending RenderFrameHost is created
  // for a cross-site navigation, because we must suspend any navigations until
  // we hear back from the old renderer's onbeforeunload handler. Note that it
  // is important that only one navigation event happen after calling this
  // method with |suspend| equal to true. If |suspend| is false and there is a
  // suspended_nav_message_, this will send the message. This function should
  // only be called to toggle the state; callers should check
  // are_navigations_suspended() first. If |suspend| is false, the time that the
  // user decided the navigation should proceed should be passed as
  // |proceed_time|.
  void SetNavigationsSuspended(bool suspend,
                               const base::TimeTicks& proceed_time);

  // Clears any suspended navigation state after a cross-site navigation is
  // canceled or suspended. This is important if we later return to this
  // RenderFrameHost.
  void CancelSuspendedNavigations();

  // Runs the beforeunload handler for this frame. |for_cross_site_transition|
  // indicates whether this call is for the current frame during a cross-process
  // navigation. False means we're closing the entire tab.
  void DispatchBeforeUnload(bool for_cross_site_transition);

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  void ExtendSelectionAndDelete(size_t before, size_t after);

  // Notifies the RenderFrame that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input,
                              bool dialog_was_suppressed);

  // Called when an HTML5 notification is closed.
  void NotificationClosed(int notification_id);

  // Clears any outstanding transition request. This is called when we hear the
  // response or commit.
  void ClearPendingTransitionRequestData();

  // Send a message to the renderer process to change the accessibility mode.
  void SetAccessibilityMode(AccessibilityMode AccessibilityMode);

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process, and the accessibility tree it sent can be
  // retrieved using GetAXTreeForTesting().
  void SetAccessibilityCallbackForTesting(
      const base::Callback<void(ui::AXEvent, int)>& callback);

  // Returns a snapshot of the accessibility tree received from the
  // renderer as of the last time an accessibility notification was
  // received.
  const ui::AXTree* GetAXTreeForTesting();

  // Access the BrowserAccessibilityManager if it already exists.
  BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // NULL.
  BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

#if defined(OS_WIN)
  void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent);
  gfx::NativeViewAccessible GetParentNativeViewAccessible() const;
#endif

 protected:
  friend class RenderFrameHostFactory;

  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int routing_id,
                      bool is_swapped_out);

 private:
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;

  // IPC Message handlers.
  void OnAddMessageToConsole(int32 level,
                             const base::string16& message,
                             int32 line_no,
                             const base::string16& source_id);
  void OnDetach();
  void OnFrameFocused();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnDocumentOnLoadCompleted();
  void OnDidStartProvisionalLoadForFrame(const GURL& url,
                                         bool is_transition_navigation);
  void OnDidFailProvisionalLoadWithError(
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidFailLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description);
  void OnDidRedirectProvisionalLoad(int32 page_id,
                                    const GURL& source_url,
                                    const GURL& target_url);
  void OnNavigate(const IPC::Message& msg);
  void OnBeforeUnloadACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnSwapOutACK();
  void OnContextMenu(const ContextMenuParams& params);
  void OnJavaScriptExecuteResponse(int id, const base::ListValue& result);
  void OnRunJavaScriptMessage(const base::string16& message,
                              const base::string16& default_prompt,
                              const GURL& frame_url,
                              JavaScriptMessageType type,
                              IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(const GURL& frame_url,
                                const base::string16& message,
                                bool is_reload,
                                IPC::Message* reply_msg);
  void OnRequestPlatformNotificationPermission(const GURL& origin,
                                               int request_id);
  void OnShowDesktopNotification(
      int notification_id,
      const ShowDesktopNotificationHostMsgParams& params);
  void OnCancelDesktopNotification(int notification_id);
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          size_t start_offset,
                                          size_t end_offset);
  void OnDidAccessInitialDocument();
  void OnDidDisownOpener();
  void OnUpdateTitle(int32 page_id,
                     const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnUpdateEncoding(const std::string& encoding);
  void OnBeginNavigation(
      const FrameHostMsg_BeginNavigation_Params& params);
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params);
  void OnAccessibilityLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  void PlatformNotificationPermissionRequestDone(
      int request_id, blink::WebNotificationPermission permission);

  // For now, RenderFrameHosts indirectly keep RenderViewHosts alive via a
  // refcount that calls Shutdown when it reaches zero.  This allows each
  // RenderFrameHostManager to just care about RenderFrameHosts, while ensuring
  // we have a RenderViewHost for each RenderFrameHost.
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  RenderViewHostImpl* render_view_host_;

  RenderFrameHostDelegate* delegate_;

  // |cross_process_frame_connector_| passes messages from an out-of-process
  // child frame to the parent process for compositing.
  //
  // This is only non-NULL when this is the swapped out RenderFrameHost in
  // the same site instance as this frame's parent.
  //
  // See the class comment above CrossProcessFrameConnector for more
  // information.
  //
  // This will move to RenderFrameProxyHost when that class is created.
  CrossProcessFrameConnector* cross_process_frame_connector_;

  // The proxy created for this RenderFrameHost. It is used to send and receive
  // IPC messages while in swapped out state.
  // TODO(nasko): This can be removed once we don't have a swapped out state on
  // RenderFrameHosts. See https://crbug.com/357747.
  RenderFrameProxyHost* render_frame_proxy_host_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  FrameTree* frame_tree_;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  FrameTreeNode* frame_tree_node_;

  // The mapping of pending JavaScript calls created by
  // ExecuteJavaScript and their corresponding callbacks.
  std::map<int, JavaScriptResultCallback> javascript_callbacks_;

  // Map from notification_id to a callback to cancel them.
  std::map<int, base::Closure> cancel_notification_callbacks_;

  int routing_id_;
  bool is_swapped_out_;
  bool renderer_initialized_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them. This will be true when a RenderFrameHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderFrameHost.
  bool navigations_suspended_;

  // We only buffer the params for a suspended navigation while this RFH is the
  // pending RenderFrameHost of a RenderFrameHostManager. There will only ever
  // be one suspended navigation, because RenderFrameHostManager will destroy
  // the pending RenderFrameHost and create a new one if a second navigation
  // occurs.
  scoped_ptr<FrameMsg_Navigate_Params> suspended_nav_params_;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  ServiceRegistryImpl service_registry_;

  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_;

  scoped_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // Callback when an event is received, for testing.
  base::Callback<void(ui::AXEvent, int)> accessibility_testing_callback_;
  // The most recently received accessibility tree - for testing only.
  scoped_ptr<ui::AXTree> ax_tree_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
