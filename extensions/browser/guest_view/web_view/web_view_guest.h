// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_

#include <vector>

#include "base/observer_list.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/guest_view/guest_view.h"
#include "extensions/browser/guest_view/web_view/javascript_dialog_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_find_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "extensions/browser/script_executor.h"

namespace blink {
struct WebFindOptions;
}  // nanespace blink

namespace extensions {

class WebViewInternalFindFunction;

// A WebViewGuest provides the browser-side implementation of the <webview> API
// and manages the dispatch of <webview> extension events. WebViewGuest is
// created on attachment. That is, when a guest WebContents is associated with
// a particular embedder WebContents. This happens on either initial navigation
// or through the use of the New Window API, when a new window is attached to
// a particular <webview>.
class WebViewGuest : public GuestView<WebViewGuest>,
                     public content::NotificationObserver {
 public:
  static GuestViewBase* Create(content::WebContents* owner_web_contents);

  // For WebViewGuest, we create special guest processes, which host the
  // tag content separately from the main application that embeds the tag.
  // A <webview> can specify both the partition name and whether the storage
  // for that partition should be persisted. Each tag gets a SiteInstance with
  // a specially formatted URL, based on the application it is hosted by and
  // the partition requested by it. The format for that URL is:
  // chrome-guest://partition_domain/persist?partition_name
  static bool GetGuestPartitionConfigForSite(const GURL& site,
                                             std::string* partition_domain,
                                             std::string* partition_name,
                                             bool* in_memory);

  // Returns guestview::kInstanceIDNone if |contents| does not correspond to a
  // WebViewGuest.
  static int GetViewInstanceId(content::WebContents* contents);

  static const char Type[];

  // Returns the stored rules registry ID of the given webview. Will generate
  // an ID for the first query.
  static int GetOrGenerateRulesRegistryID(
      int embedder_process_id,
      int web_view_instance_id);

  // Request navigating the guest to the provided |src| URL.
  void NavigateGuest(const std::string& src, bool force_navigation);

  // Shows the context menu for the guest.
  // |items| acts as a filter. This restricts the current context's default
  // menu items to contain only the items from |items|.
  // |items| == NULL means no filtering will be applied.
  void ShowContextMenu(
      int request_id,
      const WebViewGuestDelegate::MenuItemVector* items);

  // Sets the frame name of the guest.
  void SetName(const std::string& name);

  // Set the zoom factor.
  void SetZoom(double zoom_factor);

  void SetAllowScaling(bool allow);

  // Sets the transparency of the guest.
  void SetAllowTransparency(bool allow);

  // Loads a data URL with a specified base URL and virtual URL.
  bool LoadDataWithBaseURL(const std::string& data_url,
                           const std::string& base_url,
                           const std::string& virtual_url,
                           std::string* error);

  // GuestViewBase implementation.
  bool CanRunInDetachedState() const override;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) override;
  void DidAttachToEmbedder() override;
  void DidDropLink(const GURL& url) override;
  void DidInitialize(const base::DictionaryValue& create_params) override;
  void DidStopLoading() override;
  void EmbedderFullscreenToggled(bool entered_fullscreen) override;
  void EmbedderWillBeDestroyed() override;
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;
  void GuestDestroyed() override;
  void GuestReady() override;
  void GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                     const gfx::Size& new_size) override;
  bool IsAutoSizeSupported() const override;
  bool IsDragAndDropEnabled() const override;
  void WillAttachToEmbedder() override;
  void WillDestroy() override;

  // WebContentsDelegate implementation.
  bool AddMessageToConsole(content::WebContents* source,
                           int32 level,
                           const base::string16& message,
                           int32 line_no,
                           const base::string16& source_id) override;
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;
  void CloseContents(content::WebContents* source) override;
  void FindReply(content::WebContents* source,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  void RendererResponsive(content::WebContents* source) override;
  void RendererUnresponsive(content::WebContents* source) override;
  void RequestMediaAccessPermission(
      content::WebContents* source,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
  void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) override;
  bool CheckMediaAccessPermission(content::WebContents* source,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;
  void CanDownload(content::RenderViewHost* render_view_host,
                   const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_frame_id,
                          const base::string16& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void EnterFullscreenModeForTab(content::WebContents* web_contents,
                                 const GURL& origin) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const override;

  // NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns the current zoom factor.
  double zoom() const { return current_zoom_factor_; }

  // Begin or continue a find request.
  void StartFindInternal(
      const base::string16& search_text,
      const blink::WebFindOptions& options,
      scoped_refptr<WebViewInternalFindFunction> find_function);

  // Conclude a find request to clear highlighting.
  void StopFindingInternal(content::StopFindAction);

  // If possible, navigate the guest to |relative_index| entries away from the
  // current navigation entry. Returns true on success.
  bool Go(int relative_index);

  // Reload the guest.
  void Reload();

  using PermissionResponseCallback =
      base::Callback<void(bool /* allow */,
                          const std::string& /* user_input */)>;
  int RequestPermission(
      WebViewPermissionType permission_type,
      const base::DictionaryValue& request_info,
      const PermissionResponseCallback& callback,
      bool allowed_by_default);

  // Requests Geolocation Permission from the embedder.
  void RequestGeolocationPermission(int bridge_id,
                                    const GURL& requesting_frame,
                                    bool user_gesture,
                                    const base::Callback<void(bool)>& callback);
  void CancelGeolocationPermissionRequest(int bridge_id);

  // Called when file system access is requested by the guest content using the
  // HTML5 file system API in main thread, or a worker thread.
  // The request is plumbed through the <webview> permission request API. The
  // request will be:
  // - Allowed if the embedder explicitly allowed it.
  // - Denied if the embedder explicitly denied.
  // - Determined by the guest's content settings if the embedder does not
  // perform an explicit action.
  void RequestFileSystemPermission(const GURL& url,
                                   bool allowed_by_default,
                                   const base::Callback<void(bool)>& callback);

  // Overrides the user agent for this guest.
  // This affects subsequent guest navigations.
  void SetUserAgentOverride(const std::string& user_agent_override);

  // Stop loading the guest.
  void Stop();

  // Kill the guest process.
  void Terminate();

  // Clears data in the storage partition of this guest.
  //
  // Partition data that are newer than |removal_since| will be removed.
  // |removal_mask| corresponds to bitmask in StoragePartition::RemoveDataMask.
  bool ClearData(const base::Time remove_since,
                 uint32 removal_mask,
                 const base::Closure& callback);

  ScriptExecutor* script_executor() { return script_executor_.get(); }

 private:
  friend class WebViewPermissionHelper;

  explicit WebViewGuest(content::WebContents* owner_web_contents);

  ~WebViewGuest() override;

  void AttachWebViewHelpers(content::WebContents* contents);

  void OnWebViewNewWindowResponse(int new_window_instance_id,
                                  bool allow,
                                  const std::string& user_input);

  void OnFullscreenPermissionDecided(bool allowed,
                                     const std::string& user_input);
  bool GuestMadeEmbedderFullscreen() const;
  void SetFullscreenState(bool is_fullscreen);

  // WebContentsObserver implementation.
  void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  void DidFailProvisionalLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url,
                              int error_code,
                              const base::string16& error_description) override;
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void UserAgentOverrideSet(const std::string& user_agent) override;
  void FrameNameChanged(content::RenderFrameHost* render_frame_host,
                        const std::string& name) override;

  // Informs the embedder of a frame name change.
  void ReportFrameNameChange(const std::string& name);

  // Called after the load handler is called in the guest's main frame.
  void LoadHandlerCalled();

  // Called when a redirect notification occurs.
  void LoadRedirect(const GURL& old_url,
                    const GURL& new_url,
                    bool is_top_level);

  void PushWebViewStateToIOThread();
  static void RemoveWebViewStateFromIOThread(
      content::WebContents* web_contents);

  // Loads the |url| provided. |force_navigation| indicates whether to reload
  // the content if the provided |url| matches the current page of the guest.
  void LoadURLWithParams(const GURL& url,
                         const content::Referrer& referrer,
                         ui::PageTransition transition_type,
                         bool force_navigation);

  void RequestNewWindowPermission(
      WindowOpenDisposition disposition,
      const gfx::Rect& initial_bounds,
      bool user_gesture,
      content::WebContents* new_contents);

  // Requests resolution of a potentially relative URL.
  GURL ResolveURL(const std::string& src);

  // Notification that a load in the guest resulted in abort. Note that |url|
  // may be invalid.
  void LoadAbort(bool is_top_level,
                 const GURL& url,
                 const std::string& error_type);

  // Creates a new guest window owned by this WebViewGuest.
  void CreateNewGuestWebViewWindow(const content::OpenURLParams& params);

  void NewGuestWebViewCallback(const content::OpenURLParams& params,
                               content::WebContents* guest_web_contents);

  bool HandleKeyboardShortcuts(const content::NativeWebKeyboardEvent& event);

  void ApplyAttributes(const base::DictionaryValue& params);

  // Identifies the set of rules registries belonging to this guest.
  int rules_registry_id_;

  // Handles find requests and replies for the webview find API.
  WebViewFindHelper find_helper_;

  ObserverList<ScriptExecutionObserver> script_observers_;
  scoped_ptr<ScriptExecutor> script_executor_;

  content::NotificationRegistrar notification_registrar_;

  // True if the user agent is overridden.
  bool is_overriding_user_agent_;

  // Stores the window name of the main frame of the guest.
  std::string name_;

  // Stores whether the contents of the guest can be transparent.
  bool guest_opaque_;

  // Stores the src URL of the WebView.
  GURL src_;

  // Handles the JavaScript dialog requests.
  JavaScriptDialogHelper javascript_dialog_helper_;

  // Handles permission requests.
  scoped_ptr<WebViewPermissionHelper> web_view_permission_helper_;

  scoped_ptr<WebViewGuestDelegate> web_view_guest_delegate_;

  // Tracks the name, and target URL of the new window. Once the first
  // navigation commits, we no longer track this information.
  struct NewWindowInfo {
    GURL url;
    std::string name;
    bool changed;
    NewWindowInfo(const GURL& url, const std::string& name) :
        url(url),
        name(name),
        changed(false) {}
  };

  using PendingWindowMap = std::map<WebViewGuest*, NewWindowInfo>;
  PendingWindowMap pending_new_windows_;

  // Stores the current zoom factor.
  double current_zoom_factor_;

  // Determines if this guest accepts pinch-zoom gestures.
  bool allow_scaling_;
  bool is_guest_fullscreen_;
  bool is_embedder_fullscreen_;
  bool last_fullscreen_permission_was_allowed_by_embedder_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<WebViewGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
