// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/dom_storage_common.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/view_types.h"
#include "chrome/common/window_container_type.h"
#include "ipc/ipc_channel.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "webkit/glue/window_open_disposition.h"


class AutomationResourceRoutingDelegate;
class BackgroundContents;
struct BookmarkNodeData;
class BookmarkNode;
struct ContextMenuParams;
class FilePath;
class GURL;
class ListValue;
struct NativeWebKeyboardEvent;
class NavigationEntry;
class Profile;
struct RendererPreferences;
class RenderProcessHost;
class RenderViewHost;
class ResourceRedirectDetails;
class ResourceRequestDetails;
class SkBitmap;
class SSLClientAuthHandler;
class SSLAddCertHandler;
class TabContents;
struct ThumbnailScore;
struct ViewHostMsg_DidPrintPage_Params;
struct ViewHostMsg_DomMessage_Params;
struct ViewHostMsg_FrameNavigate_Params;
struct ViewHostMsg_PageHasOSDD_Type;
struct ViewHostMsg_RunFileChooser_Params;
struct WebApplicationInfo;
struct WebDropData;
struct WebMenuItem;
class WebKeyboardEvent;
struct WebPreferences;

namespace base {
class WaitableEvent;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace IPC {
class Message;
}

namespace net {
class CookieList;
class CookieOptions;
}

namespace webkit_glue {
struct FormData;
class FormField;
struct PasswordForm;
}

//
// RenderViewHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderViewHost.
//
//  This interface currently encompasses every type of message that was
//  previously being sent by TabContents itself. Some of these notifications
//  may not be relevant to all users of RenderViewHost and we should consider
//  exposing a more generic Send function on RenderViewHost and a response
//  listener here to serve that need.
//
class RenderViewHostDelegate : public IPC::Channel::Listener {
 public:
  // View ----------------------------------------------------------------------
  // Functions that can be routed directly to a view-specific class.

  class View {
   public:
    // The page is trying to open a new page (e.g. a popup window). The
    // window should be created associated with the given route, but it should
    // not be shown yet. That should happen in response to ShowCreatedWindow.
    // |window_container_type| describes the type of RenderViewHost container
    // that is requested -- in particular, the window.open call may have
    // specified 'background' and 'persistent' in the feature string.
    //
    // The passed |frame_name| parameter is the name parameter that was passed
    // to window.open(), and will be empty if none was passed.
    //
    // Note: this is not called "CreateWindow" because that will clash with
    // the Windows function which is actually a #define.
    //
    // NOTE: this takes ownership of @modal_dialog_event
    virtual void CreateNewWindow(
        int route_id,
        WindowContainerType window_container_type,
        const string16& frame_name) = 0;

    // The page is trying to open a new widget (e.g. a select popup). The
    // widget should be created associated with the given route, but it should
    // not be shown yet. That should happen in response to ShowCreatedWidget.
    // |popup_type| indicates if the widget is a popup and what kind of popup it
    // is (select, autofill...).
    virtual void CreateNewWidget(int route_id,
                                 WebKit::WebPopupType popup_type) = 0;

    // Creates a full screen RenderWidget. Similar to above.
    virtual void CreateNewFullscreenWidget(
        int route_id, WebKit::WebPopupType popup_type) = 0;

    // Show a previously created page with the specified disposition and bounds.
    // The window is identified by the route_id passed to CreateNewWindow.
    //
    // Note: this is not called "ShowWindow" because that will clash with
    // the Windows function which is actually a #define.
    virtual void ShowCreatedWindow(int route_id,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture) = 0;

    // Show the newly created widget with the specified bounds.
    // The widget is identified by the route_id passed to CreateNewWidget.
    virtual void ShowCreatedWidget(int route_id,
                                   const gfx::Rect& initial_pos) = 0;

    // Show the newly created full screen widget. Similar to above.
    virtual void ShowCreatedFullscreenWidget(int route_id) = 0;

    // A context menu should be shown, to be built using the context information
    // provided in the supplied params.
    virtual void ShowContextMenu(const ContextMenuParams& params) = 0;

    // Shows a popup menu with the specified items.
    // This method should call RenderViewHost::DidSelectPopupMenuItemAt() or
    // RenderViewHost::DidCancelPopupMenu() ased on the user action.
    virtual void ShowPopupMenu(const gfx::Rect& bounds,
                               int item_height,
                               double item_font_size,
                               int selected_item,
                               const std::vector<WebMenuItem>& items,
                               bool right_aligned) = 0;

    // The user started dragging content of the specified type within the
    // RenderView. Contextual information about the dragged content is supplied
    // by WebDropData.
    virtual void StartDragging(const WebDropData& drop_data,
                               WebKit::WebDragOperationsMask allowed_ops,
                               const SkBitmap& image,
                               const gfx::Point& image_offset) = 0;

    // The page wants to update the mouse cursor during a drag & drop operation.
    // |operation| describes the current operation (none, move, copy, link.)
    virtual void UpdateDragCursor(WebKit::WebDragOperation operation) = 0;

    // Notification that view for this delegate got the focus.
    virtual void GotFocus() = 0;

    // Callback to inform the browser that the page is returning the focus to
    // the browser's chrome. If reverse is true, it means the focus was
    // retrieved by doing a Shift-Tab.
    virtual void TakeFocus(bool reverse) = 0;

    // Notification that the view has lost capture.
    virtual void LostCapture() = 0;

    // The page wants the hosting window to activate/deactivate itself (it
    // called the JavaScript window.focus()/blur() method).
    virtual void Activate() = 0;
    virtual void Deactivate() = 0;

    // Callback to give the browser a chance to handle the specified keyboard
    // event before sending it to the renderer.
    // Returns true if the |event| was handled. Otherwise, if the |event| would
    // be handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
    // |*is_keyboard_shortcut| should be set to true.
    virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                        bool* is_keyboard_shortcut) = 0;

    // Callback to inform the browser that the renderer did not process the
    // specified events. This gives an opportunity to the browser to process the
    // event (used for keyboard shortcuts).
    virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) = 0;

    // Notifications about mouse events in this view.  This is useful for
    // implementing global 'on hover' features external to the view.
    virtual void HandleMouseMove() = 0;
    virtual void HandleMouseDown() = 0;
    virtual void HandleMouseLeave() = 0;
    virtual void HandleMouseUp() = 0;
    virtual void HandleMouseActivate() = 0;

    // The contents' preferred size changed.
    virtual void UpdatePreferredSize(const gfx::Size& pref_size) = 0;

   protected:
    virtual ~View() {}
  };

  // RendererManagerment -------------------------------------------------------
  // Functions for managing switching of Renderers. For TabContents, this is
  // implemented by the RenderViewHostManager

  class RendererManagement {
   public:
    // Notification whether we should close the page, after an explicit call to
    // AttemptToClosePage.  This is called before a cross-site request or before
    // a tab/window is closed (as indicated by the first parameter) to allow the
    // appropriate renderer to approve or deny the request.  |proceed| indicates
    // whether the user chose to proceed.
    virtual void ShouldClosePage(bool for_cross_site_transition,
                                 bool proceed) = 0;

    // Called by ResourceDispatcherHost when a response for a pending cross-site
    // request is received.  The ResourceDispatcherHost will pause the response
    // until the onunload handler of the previous renderer is run.
    virtual void OnCrossSiteResponse(int new_render_process_host_id,
                                     int new_request_id) = 0;

    // Called the ResourceDispatcherHost's associate CrossSiteRequestHandler
    // when a cross-site navigation has been canceled.
    virtual void OnCrossSiteNavigationCanceled() = 0;

   protected:
    virtual ~RendererManagement() {}
  };

  // BrowserIntegration --------------------------------------------------------
  // Functions that integrate with other browser services.

  class BrowserIntegration {
   public:
    // Notification the user has made a gesture while focus was on the
    // page. This is used to avoid uninitiated user downloads (aka carpet
    // bombing), see DownloadRequestLimiter for details.
    virtual void OnUserGesture() = 0;

    // A find operation in the current page completed.
    virtual void OnFindReply(int request_id,
                             int number_of_matches,
                             const gfx::Rect& selection_rect,
                             int active_match_ordinal,
                             bool final_update) = 0;

    // Navigate to the history entry for the given offset from the current
    // position within the NavigationController.  Makes no change if offset is
    // not valid.
    virtual void GoToEntryAtOffset(int offset) = 0;

    // Notification when default plugin updates status of the missing plugin.
    virtual void OnMissingPluginStatus(int status) = 0;

    // Notification from the renderer that a plugin instance has crashed.
    //
    // BrowserIntegration isn't necessarily the best place for this, if you
    // need to implement this function somewhere that doesn't need any other
    // BrowserIntegration callbacks, feel free to move it elsewhere.
    virtual void OnCrashedPlugin(const FilePath& plugin_path) = 0;

    // Notification that a worker process has crashed.
    virtual void OnCrashedWorker() = 0;

    virtual void OnBlockedOutdatedPlugin(const string16& name,
                                         const GURL& update_url) = 0;

    // Notification that a user's request to install an application has
    // completed.
    virtual void OnDidGetApplicationInfo(
        int32 page_id,
        const WebApplicationInfo& app_info) = 0;

    // Notification when an application programmatically requests installation.
    virtual void OnInstallApplication(
        const WebApplicationInfo& app_info) = 0;

    // Notification that the contents of the page has been loaded.
    virtual void OnPageContents(const GURL& url,
                                int renderer_process_id,
                                int32 page_id,
                                const string16& contents,
                                const std::string& language,
                                bool page_translatable) = 0;

    // Notification that the page has been translated.
    virtual void OnPageTranslated(int32 page_id,
                                  const std::string& original_lang,
                                  const std::string& translated_lang,
                                  TranslateErrors::Type error_type) = 0;

    // Notification that the page has a suggest result.
    virtual void OnSetSuggestions(
        int32 page_id,
        const std::vector<std::string>& result) = 0;

    // Notification of whether the page supports instant-style interaction.
    virtual void OnInstantSupportDetermined(int32 page_id, bool result) = 0;

   protected:
    virtual ~BrowserIntegration() {}
  };

  // ContentSettings------------------------------------------------------------
  // Interface for content settings related events.

  class ContentSettings {
   public:
    // Called when content in the current page was blocked due to the user's
    // content settings.
    virtual void OnContentBlocked(ContentSettingsType type,
                                  const std::string& resource_identifier) = 0;

    // Called when cookies for the given URL were read either from within the
    // current page or while loading it. |blocked_by_policy| should be true, if
    // reading cookies was blocked due to the user's content settings. In that
    // case, this function should invoke OnContentBlocked.
    virtual void OnCookiesRead(
        const GURL& url,
        const net::CookieList& cookie_list,
        bool blocked_by_policy) = 0;

    // Called when a specific cookie in the current page was changed.
    // |blocked_by_policy| should be true, if the cookie was blocked due to the
    // user's content settings. In that case, this function should invoke
    // OnContentBlocked.
    virtual void OnCookieChanged(const GURL& url,
                                 const std::string& cookie_line,
                                 const net::CookieOptions& options,
                                 bool blocked_by_policy) = 0;

    // Called when a specific indexed db factory in the current page was
    // accessed. If access was blocked due to the user's content settings,
    // |blocked_by_policy| should be true, and this function should invoke
    // OnContentBlocked.
    virtual void OnIndexedDBAccessed(const GURL& url,
                                     const string16& description,
                                     bool blocked_by_policy) = 0;

    // Called when a specific local storage area in the current page was
    // accessed. If access was blocked due to the user's content settings,
    // |blocked_by_policy| should be true, and this function should invoke
    // OnContentBlocked.
    virtual void OnLocalStorageAccessed(const GURL& url,
                                        DOMStorageType storage_type,
                                        bool blocked_by_policy) = 0;

    // Called when a specific Web database in the current page was accessed. If
    // access was blocked due to the user's content settings,
    // |blocked_by_policy| should eb true, and this function should invoke
    // OnContentBlocked.
    virtual void OnWebDatabaseAccessed(const GURL& url,
                                       const string16& name,
                                       const string16& display_name,
                                       unsigned long estimated_size,
                                       bool blocked_by_policy) = 0;

    // Called when a specific appcache in the current page was accessed. If
    // access was blocked due to the user's content settings,
    // |blocked_by_policy| should eb true, and this function should invoke
    // OnContentBlocked.
    virtual void OnAppCacheAccessed(const GURL& manifest_url,
                                    bool blocked_by_policy) = 0;

    // Called when geolocation permission was set in a frame on the current
    // page.
    virtual void OnGeolocationPermissionSet(const GURL& requesting_frame,
                                            bool allowed) = 0;

   protected:
    virtual ~ContentSettings() {}
  };

  // Save ----------------------------------------------------------------------
  // Interface for saving web pages.

  class Save {
   public:
    // Notification that we get when we receive all savable links of
    // sub-resources for the current page, their referrers and list of frames
    // (include main frame and sub frames).
    virtual void OnReceivedSavableResourceLinksForCurrentPage(
        const std::vector<GURL>& resources_list,
        const std::vector<GURL>& referrers_list,
        const std::vector<GURL>& frames_list) = 0;

    // Notification that we get when we receive serialized html content data of
    // a specified web page from render process. The parameter frame_url
    // specifies what frame the data belongs. The parameter data contains the
    // available data for sending. The parameter status indicates the
    // serialization status, See
    // webkit_glue::DomSerializerDelegate::PageSavingSerializationStatus for
    // the detail meaning of status.
    virtual void OnReceivedSerializedHtmlData(const GURL& frame_url,
                                              const std::string& data,
                                              int32 status) = 0;

   protected:
    virtual ~Save() {}
  };

  // Printing ------------------------------------------------------------------

  class Printing {
   public:
    // Notification that the render view has calculated the number of printed
    // pages.
    virtual void DidGetPrintedPagesCount(int cookie, int number_pages) = 0;

    // Notification that the render view is done rendering one printed page.
    // This call is synchronous, the renderer is waiting on us because of the
    // EMF memory mapped data.
    virtual void DidPrintPage(
        const ViewHostMsg_DidPrintPage_Params& params) = 0;

   protected:
    virtual ~Printing() {}
  };

  // FavIcon -------------------------------------------------------------------
  // Interface for the renderer to supply favicon information.

  class FavIcon {
   public:
    // An image that was requested to be downloaded by DownloadImage has
    // completed.
    //
    // TODO(brettw) this should be renamed DidDownloadFavIcon, and the RVH
    // function, IPC message, and the RenderView function DownloadImage should
    // all be named DownloadFavIcon.
    virtual void DidDownloadFavIcon(RenderViewHost* render_view_host,
                                    int id,
                                    const GURL& image_url,
                                    bool errored,
                                    const SkBitmap& image) = 0;

    // The URL for the FavIcon of a page has changed.
    virtual void UpdateFavIconURL(RenderViewHost* render_view_host,
                                  int32 page_id,
                                  const GURL& icon_url) = 0;

   protected:
    virtual ~FavIcon() {}
  };

  // BookmarkDrag --------------------------------------------------------------
  // Interface for forwarding bookmark drag and drop to extenstions.

  class BookmarkDrag {
   public:
    virtual void OnDragEnter(const BookmarkNodeData& data) = 0;
    virtual void OnDragOver(const BookmarkNodeData& data) = 0;
    virtual void OnDragLeave(const BookmarkNodeData& data) = 0;
    virtual void OnDrop(const BookmarkNodeData& data) = 0;

   protected:
    virtual ~BookmarkDrag() {}
  };

  // SSL -----------------------------------------------------------------------
  // Interface for UI and other RenderViewHost-specific interactions with SSL.

  class SSL {
   public:
    // Displays a dialog to select client certificates from |request_info|,
    // returning them to |handler|.
    virtual void ShowClientCertificateRequestDialog(
        scoped_refptr<SSLClientAuthHandler> handler) = 0;

    // Called when |handler| encounters an error in verifying a
    // received client certificate. Note that, because CAs often will
    // not send us intermediate certificates, the verification we can
    // do is minimal: we verify the certificate is parseable, that we
    // have the corresponding private key, and that the certificate
    // has not expired.
    virtual void OnVerifyClientCertificateError(
        scoped_refptr<SSLAddCertHandler> handler, int error_code) = 0;

    // Called when |handler| requests the user's confirmation in adding a
    // client certificate.
    virtual void AskToAddClientCertificate(
        scoped_refptr<SSLAddCertHandler> handler) = 0;

    // Called when |handler| successfully adds a client certificate.
    virtual void OnAddClientCertificateSuccess(
        scoped_refptr<SSLAddCertHandler> handler) = 0;

    // Called when |handler| encounters an error adding a client certificate.
    virtual void OnAddClientCertificateError(
        scoped_refptr<SSLAddCertHandler> handler, int error_code) = 0;

    // Called when |handler| has completed, so the delegate may release any
    // state accumulated.
    virtual void OnAddClientCertificateFinished(
        scoped_refptr<SSLAddCertHandler> handler) = 0;

   protected:
    virtual ~SSL() {}
  };

  // FileSelect ----------------------------------------------------------------
  // Interface for handling file selection.

  class FileSelect {
   public:
    // A file chooser should be shown.
    virtual void RunFileChooser(
        RenderViewHost* render_view_host,
        const ViewHostMsg_RunFileChooser_Params& params) = 0;

   protected:
    virtual ~FileSelect() {}
  };

  // ---------------------------------------------------------------------------

  // Returns the current delegate associated with a feature. May return NULL if
  // there is no corresponding delegate.
  virtual View* GetViewDelegate();
  virtual RendererManagement* GetRendererManagementDelegate();
  virtual BrowserIntegration* GetBrowserIntegrationDelegate();
  virtual ContentSettings* GetContentSettingsDelegate();
  virtual Save* GetSaveDelegate();
  virtual Printing* GetPrintingDelegate();
  virtual FavIcon* GetFavIconDelegate();

  virtual BookmarkDrag* GetBookmarkDragDelegate();
  virtual SSL* GetSSLDelegate();
  virtual FileSelect* GetFileSelectDelegate();

  // Return the delegate for registering RenderViewHosts for automation resource
  // routing.
  virtual AutomationResourceRoutingDelegate*
      GetAutomationResourceRoutingDelegate();

  // IPC::Channel::Listener implementation.
  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Gets the URL that is currently being displayed, if there is one.
  virtual const GURL& GetURL() const;

  // Return this object cast to a TabContents, if it is one. If the object is
  // not a TabContents, returns NULL.
  virtual TabContents* GetAsTabContents();

  // Return this object cast to a BackgroundContents, if it is one. If the
  // object is not a BackgroundContents, returns NULL.
  virtual BackgroundContents* GetAsBackgroundContents();

  // Return id number of browser window which this object is attached to. If no
  // browser window is attached to, just return -1.
  virtual int GetBrowserWindowID() const = 0;

  // Return type of RenderView which is attached with this object.
  virtual ViewType::Type GetRenderViewType() const = 0;

  // The RenderView is being constructed (message sent to the renderer process
  // to construct a RenderView).  Now is a good time to send other setup events
  // to the RenderView.  This precedes any other commands to the RenderView.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // The RenderView has been constructed.
  virtual void RenderViewReady(RenderViewHost* render_view_host) {}

  // The RenderView died somehow (crashed or was killed by the user).
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) {}

  // The RenderView is going to be deleted. This is called when each
  // RenderView is going to be destroyed
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // The RenderView was navigated to a different page.
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params) {}

  // The state for the page changed and should be updated.
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state) {}

  // The page's title was changed and should be updated.
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title) {}

  // The page's encoding was changed and should be updated.
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::string& encoding) {}

  // The destination URL has changed should be updated
  virtual void UpdateTargetURL(int32 page_id, const GURL& url) {}

  // The thumbnail representation of the page changed and should be updated.
  virtual void UpdateThumbnail(const GURL& url,
                               const SkBitmap& bitmap,
                               const ThumbnailScore& score) {}

  // Inspector setting was changed and should be persisted.
  virtual void UpdateInspectorSetting(const std::string& key,
                                      const std::string& value) = 0;

  virtual void ClearInspectorSettings() = 0;

  // The page is trying to close the RenderView's representation in the client.
  virtual void Close(RenderViewHost* render_view_host) {}

  // The page is trying to move the RenderView's representation in the client.
  virtual void RequestMove(const gfx::Rect& new_bounds) {}

  // The RenderView began loading a new page. This corresponds to WebKit's
  // notion of the throbber starting.
  virtual void DidStartLoading() {}

  // The RenderView stopped loading a page. This corresponds to WebKit's
  // notion of the throbber stopping.
  virtual void DidStopLoading() {}

  // The RenderView made progress loading a page's top frame.
  // |progress| is a value between 0 (nothing loaded) to 1.0 (top frame
  // entirely loaded).
  virtual void DidChangeLoadProgress(double progress) {}

  // The RenderView's main frame document element is ready. This happens when
  // the document has finished parsing.
  virtual void DocumentAvailableInMainFrame(RenderViewHost* render_view_host) {}

  // The onload handler in the RenderView's main frame has completed.
  virtual void DocumentOnLoadCompletedInMainFrame(
      RenderViewHost* render_view_host,
      int32 page_id) {}

  // The page wants to open a URL with the specified disposition.
  virtual void RequestOpenURL(const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition) {}

  // A DOM automation operation completed. The result of the operation is
  // expressed in a json string.
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id) {}

  // A message was sent from HTML-based UI.
  // By default we ignore such messages.
  virtual void ProcessDOMUIMessage(
      const ViewHostMsg_DomMessage_Params& params) {}

  // A message for external host. By default we ignore such messages.
  // |receiver| can be a receiving script and |message| is any
  // arbitrary string that makes sense to the receiver.
  virtual void ProcessExternalHostMessage(const std::string& message,
                                          const std::string& origin,
                                          const std::string& target) {}

  // A javascript message, confirmation or prompt should be shown.
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message) {}

  virtual void RunBeforeUnloadConfirm(const std::wstring& message,
                                      IPC::Message* reply_msg) {}

  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   IPC::Message* reply_msg) {}

  // Notification that the page has an OpenSearch description document.
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id, const GURL& doc_url,
                           const ViewHostMsg_PageHasOSDD_Type& provider_type) {}

  // |url| is assigned to a server that can provide alternate error pages.  If
  // the returned URL is empty, the default error page built into WebKit will
  // be used.
  virtual GURL GetAlternateErrorPageURL() const;

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const = 0;

  // Returns a WebPreferences object that will be used by the renderer
  // associated with the owning render view host.
  virtual WebPreferences GetWebkitPrefs();

  // Notification from the renderer host that blocked UI event occurred.
  // This happens when there are tab-modal dialogs. In this case, the
  // notification is needed to let us draw attention to the dialog (i.e.
  // refocus on the modal dialog, flash title etc).
  virtual void OnIgnoredUIEvent() {}

  // Notification from the renderer that JS runs out of memory.
  virtual void OnJSOutOfMemory() {}

  // Notification that the renderer has become unresponsive. The
  // delegate can use this notification to show a warning to the user.
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload) {}

  // Notification that a previously unresponsive renderer has become
  // responsive again. The delegate can use this notification to end the
  // warning shown to the user.
  virtual void RendererResponsive(RenderViewHost* render_view_host) {}

  // Notification that the RenderViewHost's load state changed.
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state,
                                uint64 upload_position, uint64 upload_size) {}

  // Returns true if this view is used to host an external tab container.
  virtual bool IsExternalTabContainer() const;

  // The RenderView has inserted one css file into page.
  virtual void DidInsertCSS() {}

  // A different node in the page got focused.
  virtual void FocusedNodeChanged(bool is_editable_node) {}

  // Updates the minimum and maximum zoom percentages.
  virtual void UpdateZoomLimits(int minimum_percent,
                                int maximum_percent,
                                bool remember) {}

  // Update the content restrictions, i.e. disable print/copy.
  virtual void UpdateContentRestrictions(int restrictions) {}

 protected:
  virtual ~RenderViewHostDelegate() {}
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
