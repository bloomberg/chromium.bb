// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_replication_state.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/javascript_message_type.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_ANDROID)
#include "content/browser/mojo/service_registry_android.h"
#endif

class GURL;
struct AccessibilityHostMsg_EventParams;
struct AccessibilityHostMsg_FindInPageResultParams;
struct AccessibilityHostMsg_LocationChangeParams;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;
struct FrameHostMsg_OpenURL_Params;
#if defined(OS_MACOSX) || defined(OS_ANDROID)
struct FrameHostMsg_ShowPopup_Params;
#endif

namespace base {
class FilePath;
class ListValue;
}

namespace content {

class CrossProcessFrameConnector;
class CrossSiteTransferringRequest;
class FrameTree;
class FrameTreeNode;
class PermissionServiceContext;
class RenderFrameHostDelegate;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostDelegate;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class ResourceRequestBody;
class StreamHandle;
class TimeoutMonitor;
struct ContextMenuParams;
struct GlobalRequestID;
struct Referrer;
struct ResourceResponse;
struct TransitionLayerData;

// Flag arguments for RenderFrameHost creation.
enum CreateRenderFrameFlags {
  // The RFH will be initially placed on the swapped out hosts list.
  CREATE_RF_SWAPPED_OUT = 1 << 0,
  // The new RenderFrame is being created for a navigation of the
  // top-level frame.
  CREATE_RF_FOR_MAIN_FRAME_NAVIGATION = 1 << 1,
  // The RenderFrame is initially hidden.
  CREATE_RF_HIDDEN = 1 << 2,
  // The RenderFrameHost will have a new RenderWidgetHost created and
  // attached to it. This is used when the RenderFrameHost is in a different
  // process from its parent frame.
  CREATE_RF_NEEDS_RENDER_WIDGET_HOST = 1 << 3
};

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      public BrowserAccessibilityDelegate {
 public:
  // Keeps track of the state of the RenderFrameHostImpl, particularly with
  // respect to swap out.
  enum RenderFrameHostImplState {
    // The standard state for a RFH handling the communication with an active
    // RenderFrame.
    STATE_DEFAULT = 0,
    // The RFH has not received the SwapOutACK yet, but the new page has
    // committed in a different RFH.  Upon reception of the SwapOutACK, the RFH
    // will either enter STATE_SWAPPED_OUT (if it is a main frame and there are
    // other active frames in its SiteInstance) or it will be deleted.
    STATE_PENDING_SWAP_OUT,
    // The RFH is swapped out and stored inside a RenderFrameProxyHost, being
    // used as a placeholder to allow cross-process communication.  Only main
    // frames can enter this state.
    STATE_SWAPPED_OUT,
  };
  // Helper function to determine whether the RFH state should contribute to the
  // number of active frames of a SiteInstance or not.
  static bool IsRFHStateActive(RenderFrameHostImplState rfh_state);

  // An accessibility reset is only allowed to prevent very rare corner cases
  // or race conditions where the browser and renderer get out of sync. If
  // this happens more than this many times, kill the renderer.
  static const int kMaxAccessibilityResets = 5;

  static RenderFrameHostImpl* FromID(int process_id, int routing_id);

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  int GetRoutingID() override;
  SiteInstanceImpl* GetSiteInstance() override;
  RenderProcessHost* GetProcess() override;
  RenderFrameHost* GetParent() override;
  const std::string& GetFrameName() override;
  bool IsCrossProcessSubframe() override;
  GURL GetLastCommittedURL() override;
  gfx::NativeView GetNativeView() override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         const JavaScriptResultCallback& callback) override;
  void ExecuteJavaScriptForTests(const base::string16& javascript) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  RenderViewHost* GetRenderViewHost() override;
  ServiceRegistry* GetServiceRegistry() override;
  blink::WebPageVisibilityState GetVisibilityState() override;
  void InsertVisualStateCallback(
      const VisualStateCallback& callback) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // BrowserAccessibilityDelegate
  void AccessibilitySetFocus(int acc_obj_id) override;
  void AccessibilityDoDefaultAction(int acc_obj_id) override;
  void AccessibilityShowMenu(const gfx::Point& global_point) override;
  void AccessibilityScrollToMakeVisible(int acc_obj_id,
                                        const gfx::Rect& subfocus) override;
  void AccessibilityScrollToPoint(int acc_obj_id,
                                  const gfx::Point& point) override;
  void AccessibilitySetTextSelection(int acc_obj_id,
                                     int start_offset,
                                     int end_offset) override;
  void AccessibilitySetValue(int acc_obj_id, const base::string16& value)
      override;
  bool AccessibilityViewHasFocus() const override;
  gfx::Rect AccessibilityGetViewBounds() const override;
  gfx::Point AccessibilityOriginInScreen(
      const gfx::Rect& bounds) const override;
  void AccessibilityHitTest(const gfx::Point& point) override;
  void AccessibilitySetAccessibilityFocus(int acc_obj_id) override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  BrowserAccessibilityManager* AccessibilityGetChildFrame(
      int accessibility_node_id) override;
  void AccessibilityGetAllChildFrames(
      std::vector<BrowserAccessibilityManager*>* child_frames) override;
  BrowserAccessibility* AccessibilityGetParentFrame() override;

  // Creates a RenderFrame in the renderer process.  Only called for
  // cross-process subframe navigations in --site-per-process.
  bool CreateRenderFrame(int parent_routing_id, int proxy_routing_id);

  // Returns whether the RenderFrame in the renderer process has been created
  // and still has a connection.  This is valid for all frames.
  bool IsRenderFrameLive();

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  This is currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  void SetRenderFrameCreated(bool created);

  // Called for renderer-created windows to resume requests from this frame,
  // after they are blocked in RenderWidgetHelper::CreateNewWindow.
  void Init();

  int routing_id() const { return routing_id_; }
  void OnCreateChildFrame(int new_routing_id,
                          const std::string& frame_name,
                          SandboxFlags sandbox_flags);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // This returns the RenderFrameHost's owned RenderWidgetHost if it has one,
  // or else it returns nullptr.
  // If the RenderFrameHost is the page's main frame, this returns instead a
  // pointer to the RenderViewHost (which inherits RenderWidgetHost).
  RenderWidgetHostImpl* GetRenderWidgetHost();

  // This returns the RenderWidgetHostView that can be used to control
  // focus and visibility for this frame.
  RenderWidgetHostView* GetView();

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
      ui::PageTransition page_transition,
      bool should_replace_current_entry);

  // Called on the current RenderFrameHost when the network response is first
  // receieved.
  void OnDeferredAfterResponseStarted(
      const GlobalRequestID& global_request_id,
      const TransitionLayerData& transition_data);

  // Tells the renderer that this RenderFrame is being swapped out for one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // RenderFrameProxy to replace the RenderFrame and set it to |is_loading|
  // state. The renderer should preserve the RenderFrameProxy object until it
  // exits, in case we come back.  The renderer can exit if it has no other
  // active RenderFrames, but not until WasSwappedOut is called.
  void SwapOut(RenderFrameProxyHost* proxy, bool is_loading);

  // Whether an ongoing navigation is waiting for a BeforeUnload ACK from the
  // RenderFrame. Currently this only happens in cross-site navigations.
  // PlzNavigate: this happens in every browser-initiated navigation that is not
  // same-page.
  bool IsWaitingForBeforeUnloadACK() const;

  // Whether the RFH is waiting for an unload ACK from the renderer.
  bool IsWaitingForUnloadACK() const;

  // Whether sudden termination is allowed for this frame. This is true if there
  // are no BeforeUnload handlers or no Unload handlers registered for the
  // frame, or it was overriden by the browser to be always true.
  bool SuddenTerminationAllowed() const;

  // Called by the browser to override (or not) the sudden termination status of
  // the frame. When overriden, sudden termination is always allowed, even if
  // it would otherwise be prevented.
  void set_override_sudden_termination_status(bool enabled) {
    override_sudden_termination_status_ = enabled;
  }

  // Called when either the SwapOut request has been acknowledged or has timed
  // out.
  void OnSwappedOut();

  // Whether this RenderFrameHost has been swapped out, such that the frame is
  // now rendered by a RenderFrameHost in a different process.
  bool is_swapped_out() const { return rfh_state_ == STATE_SWAPPED_OUT; }

  // The current state of this RFH.
  RenderFrameHostImplState rfh_state() const { return rfh_state_; }

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const CommonNavigationParams& common_params,
                const StartNavigationParams& start_params,
                const CommitNavigationParams& commit_params,
                const HistoryNavigationParams& history_params);

  // Load the specified URL; this is a shortcut for Navigate().
  void NavigateToURL(const GURL& url);

  // Treat this prospective navigation as though it originated from the frame.
  // Used, e.g., for a navigation request that originated from a RemoteFrame.
  // |source_site_instance| is the SiteInstance of the frame that initiated the
  // navigation.
  // TODO(creis): Remove this method and have RenderFrameProxyHost call
  // RequestOpenURL with its FrameTreeNode.
  void OpenURL(const FrameHostMsg_OpenURL_Params& params,
               SiteInstance* source_site_instance);

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

  // Runs the beforeunload handler for this frame. |for_navigation| indicates
  // whether this call is for the current frame during a cross-process
  // navigation. False means we're closing the entire tab.
  // PlzNavigate: this call happens on all browser-initiated navigations.
  void DispatchBeforeUnload(bool for_navigation);

  // Set the frame's opener to null in the renderer process in response to an
  // action in another renderer process.
  void DisownOpener();

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  void ExtendSelectionAndDelete(size_t before, size_t after);

  // Notifies the RenderFrame that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input,
                              bool dialog_was_suppressed);

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

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

#if defined(OS_WIN)
  void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent);
  gfx::NativeViewAccessible GetParentNativeViewAccessible() const;
#elif defined(OS_MACOSX)
  // Select popup menu related methods (for external popup menus).
  void DidSelectPopupMenuItem(int selected_index);
  void DidCancelPopupMenu();
#elif defined(OS_ANDROID)
  void DidSelectPopupMenuItems(const std::vector<int>& selected_indices);
  void DidCancelPopupMenu();
#endif

  // PlzNavigate: Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  void CommitNavigation(ResourceResponse* response,
                        scoped_ptr<StreamHandle> body,
                        const CommonNavigationParams& common_params,
                        const CommitNavigationParams& commit_params,
                        const HistoryNavigationParams& history_params);

  // Sets up the Mojo connection between this instance and its associated render
  // frame if it has not yet been set up.
  void SetUpMojoIfNeeded();

  // Tears down the browser-side state relating to the Mojo connection between
  // this instance and its associated render frame.
  void InvalidateMojoConnection();

  // Returns whether the frame is focused. A frame is considered focused when it
  // is the parent chain of the focused frame within the frame tree. In
  // addition, its associated RenderWidgetHost has to be focused.
  bool IsFocused();

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(SiteInstance* site_instance,
                      RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      RenderWidgetHostDelegate* rwh_delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int routing_id,
                      int flags);

 private:
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;

  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrashSubframe);

  // IPC Message handlers.
  void OnAddMessageToConsole(int32 level,
                             const base::string16& message,
                             int32 line_no,
                             const base::string16& source_id);
  void OnDetach();
  void OnFrameFocused();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnDocumentOnLoadCompleted(
      FrameMsg_UILoadMetricsReportType::Value report_type,
      base::TimeTicks ui_timestamp);
  void OnDidStartProvisionalLoadForFrame(const GURL& url,
                                         bool is_transition_navigation);
  void OnDidFailProvisionalLoadWithError(
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidFailLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description);
  void OnDidCommitProvisionalLoad(const IPC::Message& msg);
  void OnDidDropNavigation();
  void OnBeforeUnloadACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnSwapOutACK();
  void OnRenderProcessGone(int status, int error_code);
  void OnContextMenu(const ContextMenuParams& params);
  void OnJavaScriptExecuteResponse(int id, const base::ListValue& result);
  void OnVisualStateResponse(uint64 id);
  void OnRunJavaScriptMessage(const base::string16& message,
                              const base::string16& default_prompt,
                              const GURL& frame_url,
                              JavaScriptMessageType type,
                              IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(const GURL& frame_url,
                                const base::string16& message,
                                bool is_reload,
                                IPC::Message* reply_msg);
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          size_t start_offset,
                                          size_t end_offset);
  void OnDidAccessInitialDocument();
  void OnDidDisownOpener();
  void OnDidChangeName(const std::string& name);
  void OnDidAssignPageId(int32 page_id);
  void OnDidChangeSandboxFlags(int32 frame_routing_id, SandboxFlags flags);
  void OnUpdateTitle(const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnUpdateEncoding(const std::string& encoding);
  void OnBeginNavigation(const CommonNavigationParams& common_params,
                         const BeginNavigationParams& begin_params,
                         scoped_refptr<ResourceRequestBody> body);
  void OnDispatchLoad();
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params,
      int reset_token);
  void OnAccessibilityLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);
  void OnAccessibilityFindInPageResult(
      const AccessibilityHostMsg_FindInPageResultParams& params);
  void OnToggleFullscreen(bool enter_fullscreen);
  void OnBeforeUnloadHandlersPresent(bool present);
  void OnUnloadHandlersPresent(bool present);

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void OnShowPopup(const FrameHostMsg_ShowPopup_Params& params);
  void OnHidePopup();
#endif

  // Registers Mojo services that this frame host makes available.
  void RegisterMojoServices();

  // Updates the state of this RenderFrameHost and clears any waiting state
  // that is no longer relevant.
  void SetState(RenderFrameHostImplState rfh_state);

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  // Update the the singleton FrameAccessibility instance with a map
  // from accessibility node id to the frame routing id of a cross-process
  // iframe.
  void UpdateCrossProcessIframeAccessibility(
      const std::map<int32, int>& node_to_frame_routing_id_map);

  // Update the the singleton FrameAccessibility instance with a map
  // from accessibility node id to the browser plugin instance id of a
  // guest WebContents.
  void UpdateGuestFrameAccessibility(
      const std::map<int32, int>& node_to_browser_plugin_instance_id_map);

  // Asserts that the given RenderFrameHostImpl is part of the same browser
  // context (and crashes if not), then returns whether the given frame is
  // part of the same site instance.
  bool IsSameSiteInstance(RenderFrameHostImpl* other_render_frame_host);

  // Informs the content client that geolocation permissions were used.
  void DidUseGeolocationPermission();

  // For now, RenderFrameHosts indirectly keep RenderViewHosts alive via a
  // refcount that calls Shutdown when it reaches zero.  This allows each
  // RenderFrameHostManager to just care about RenderFrameHosts, while ensuring
  // we have a RenderViewHost for each RenderFrameHost.
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  RenderViewHostImpl* render_view_host_;

  RenderFrameHostDelegate* delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  scoped_refptr<SiteInstanceImpl> site_instance_;

  // The renderer process this RenderFrameHost is associated with. It is
  // equivalent to the result of site_instance_->GetProcess(), but that
  // method has the side effect of creating the process if it doesn't exist.
  // Cache a pointer to avoid unnecessary process creation.
  RenderProcessHost* process_;

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
  std::map<uint64, VisualStateCallback> visual_state_callbacks_;

  // RenderFrameHosts that need management of the rendering and input events
  // for their frame subtrees require RenderWidgetHosts. This typically
  // means frames that are rendered in different processes from their parent
  // frames.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  scoped_ptr<RenderWidgetHostImpl> render_widget_host_;

  int routing_id_;

  // The current state of this RenderFrameHost.
  RenderFrameHostImplState rfh_state_;

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  Currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  bool render_frame_created_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them. This will be true when a RenderFrameHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderFrameHost.
  bool navigations_suspended_;

  // Holds the parameters for a suspended navigation. This can only happen while
  // this RFH is the pending RenderFrameHost of a RenderFrameHostManager. There
  // will only ever be one suspended navigation, because RenderFrameHostManager
  // will destroy the pending RenderFrameHost and create a new one if a second
  // navigation occurs.
  // PlzNavigate: unused as navigations are never suspended.
  scoped_ptr<NavigationParams> suspended_nav_params_;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Used to track whether sudden termination is allowed for this frame.
  // has_beforeunload_handlers_ and has_unload_handlers_ are also used to avoid
  // asking the renderer process to run BeforeUnload or Unload during
  // navigation. The browser can set override_sudden_termination_status_ to
  // true, in which case sudden termination will be allowed. This is used when a
  // renderer executing BeforeUnload or Unload is unresponsive. All other values
  // are modified based on IPCs received from the renderer.
  bool has_beforeunload_handlers_;
  bool has_unload_handlers_;
  bool override_sudden_termination_status_;

  // Set to true when there is a pending FrameMsg_ShouldClose message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_ack_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true). Currently only cross-site navigations
  // require a beforeUnload/unload ACK.
  // PlzNavigate: all navigations require a beforeUnload ACK.
  bool unload_ack_is_for_navigation_;

  // Used to swap out or shut down this RFH when the unload event is taking too
  // long to execute, depending on the number of active frames in the
  // SiteInstance.
  scoped_ptr<TimeoutMonitor> swapout_event_monitor_timeout_;

  scoped_ptr<ServiceRegistryImpl> service_registry_;

#if defined(OS_ANDROID)
  scoped_ptr<ServiceRegistryAndroid> service_registry_android_;
#endif

  // The object managing the accessibility tree for this frame.
  scoped_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // This is nonzero if we sent an accessibility reset to the renderer and
  // we're waiting for an IPC containing this reset token (sequentially
  // assigned) and a complete replacement accessibility tree.
  int accessibility_reset_token_;

  // A count of the number of times we needed to reset accessibility, so
  // we don't keep trying to reset forever.
  int accessibility_reset_count_;

  // Callback when an event is received, for testing.
  base::Callback<void(ui::AXEvent, int)> accessibility_testing_callback_;
  // The most recently received accessibility tree - for testing only.
  scoped_ptr<ui::AXTree> ax_tree_for_testing_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_;

  // PlzNavigate: Owns the stream used in navigations to store the body of the
  // response once it has started.
  scoped_ptr<StreamHandle> stream_handle_;

  // Context shared for each PermissionService instance created for this RFH.
  scoped_ptr<PermissionServiceContext> permission_service_context_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
