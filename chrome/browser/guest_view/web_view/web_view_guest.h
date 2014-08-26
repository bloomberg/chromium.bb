// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_

#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/guest_view/web_view/javascript_dialog_helper.h"
#include "chrome/browser/guest_view/web_view/web_view_find_helper.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_helper.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_types.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/guest_view/guest_view.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"
#include "extensions/browser/script_executor.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

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
  static GuestViewBase* Create(content::BrowserContext* browser_context,
                               int guest_instance_id);

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

  // Request navigating the guest to the provided |src| URL.
  void NavigateGuest(const std::string& src);

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

  // GuestViewBase implementation.
  virtual const char* GetAPINamespace() OVERRIDE;
  virtual void CreateWebContents(
      const std::string& embedder_extension_id,
      int embedder_render_process_id,
      const base::DictionaryValue& create_params,
      const WebContentsCreatedCallback& callback) OVERRIDE;
  virtual void DidAttachToEmbedder() OVERRIDE;
  virtual void DidInitialize() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void EmbedderDestroyed() OVERRIDE;
  virtual void GuestDestroyed() OVERRIDE;
  virtual void GuestReady() OVERRIDE;
  virtual void GuestSizeChangedDueToAutoSize(
      const gfx::Size& old_size,
      const gfx::Size& new_size) OVERRIDE;
  virtual bool IsAutoSizeSupported() const OVERRIDE;
  virtual bool IsDragAndDropEnabled() const OVERRIDE;
  virtual void WillAttachToEmbedder() OVERRIDE;
  virtual void WillDestroy() OVERRIDE;

  // WebContentsDelegate implementation.
  virtual bool AddMessageToConsole(content::WebContents* source,
                                   int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) OVERRIDE;
  virtual void LoadProgressChanged(content::WebContents* source,
                                   double progress) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void FindReply(content::WebContents* source,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void RendererResponsive(content::WebContents* source) OVERRIDE;
  virtual void RendererUnresponsive(content::WebContents* source) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* source,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual void CanDownload(content::RenderViewHost* render_view_host,
                           const GURL& url,
                           const std::string& request_method,
                           const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual content::JavaScriptDialogManager*
      GetJavaScriptDialogManager() OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* web_contents,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void WebContentsCreated(content::WebContents* source_contents,
                                  int opener_render_frame_id,
                                  const base::string16& frame_name,
                                  const GURL& target_url,
                                  content::WebContents* new_contents) OVERRIDE;

  // BrowserPluginGuestDelegate implementation.
  virtual content::WebContents* CreateNewGuestWindow(
      const content::WebContents::CreateParams& create_params) OVERRIDE;
  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns the current zoom factor.
  double GetZoom();

  // Begin or continue a find request.
  void Find(
      const base::string16& search_text,
      const blink::WebFindOptions& options,
      scoped_refptr<WebViewInternalFindFunction> find_function);

  // Conclude a find request to clear highlighting.
  void StopFinding(content::StopFindAction);

  // If possible, navigate the guest to |relative_index| entries away from the
  // current navigation entry.
  void Go(int relative_index);

  // Reload the guest.
  void Reload();

  typedef base::Callback<void(bool /* allow */,
                              const std::string& /* user_input */)>
      PermissionResponseCallback;
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
  WebViewGuest(content::BrowserContext* browser_context,
               int guest_instance_id);

  virtual ~WebViewGuest();

  void AttachWebViewHelpers(content::WebContents* contents);

  void OnWebViewNewWindowResponse(int new_window_instance_id,
                                  bool allow,
                                  const std::string& user_input);

  // WebContentsObserver implementation.
  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      content::PageTransition transition_type) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description) OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) OVERRIDE;
  virtual void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) OVERRIDE;
  virtual bool OnMessageReceived(
      const IPC::Message& message,
      content::RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void UserAgentOverrideSet(const std::string& user_agent) OVERRIDE;

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

  void LoadURLWithParams(const GURL& url,
                         const content::Referrer& referrer,
                         content::PageTransition transition_type,
                         content::WebContents* web_contents);

  void RequestNewWindowPermission(
      WindowOpenDisposition disposition,
      const gfx::Rect& initial_bounds,
      bool user_gesture,
      content::WebContents* new_contents);

  // Destroy unattached new windows that have been opened by this
  // WebViewGuest.
  void DestroyUnattachedWindows();

  // Requests resolution of a potentially relative URL.
  GURL ResolveURL(const std::string& src);

  // Notification that a load in the guest resulted in abort. Note that |url|
  // may be invalid.
  void LoadAbort(bool is_top_level,
                 const GURL& url,
                 const std::string& error_type);

  void OnFrameNameChanged(bool is_top_level, const std::string& name);

  // Creates a new guest window owned by this WebViewGuest.
  void CreateNewGuestWebViewWindow(const content::OpenURLParams& params);

  void NewGuestWebViewCallback(const content::OpenURLParams& params,
                               content::WebContents* guest_web_contents);

  bool HandleKeyboardShortcuts(const content::NativeWebKeyboardEvent& event);

  void SetUpAutoSize();

  ObserverList<ScriptExecutionObserver> script_observers_;
  scoped_ptr<ScriptExecutor> script_executor_;

  content::NotificationRegistrar notification_registrar_;

  // True if the user agent is overridden.
  bool is_overriding_user_agent_;

  // Stores the window name of the main frame of the guest.
  std::string name_;

  // Handles find requests and replies for the webview find API.
  WebViewFindHelper find_helper_;

  // Handles the JavaScript dialog requests.
  JavaScriptDialogHelper javascript_dialog_helper_;

  // Handels permission requests.
  scoped_ptr<WebViewPermissionHelper> web_view_permission_helper_;

  scoped_ptr<WebViewGuestDelegate> web_view_guest_delegate_;

  friend void WebViewFindHelper::DispatchFindUpdateEvent(bool canceled,
                                                         bool final_update);

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

  typedef std::map<WebViewGuest*, NewWindowInfo> PendingWindowMap;
  PendingWindowMap pending_new_windows_;

  DISALLOW_COPY_AND_ASSIGN(WebViewGuest);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_H_
