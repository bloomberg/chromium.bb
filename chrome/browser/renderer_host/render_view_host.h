// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/view_types.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/WebKit/chromium/public/WebTextDirection.h"
#include "webkit/glue/form_field_values.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/window_open_disposition.h"

class FilePath;
class ListValue;
class RenderViewHostDelegate;
class SiteInstance;
class SkBitmap;
class ViewMsg_Navigate;
struct ContentSettings;
struct ContextMenuParams;
struct MediaPlayerAction;
struct ThumbnailScore;
struct ViewHostMsg_DidPrintPage_Params;
struct ViewMsg_Navigate_Params;
struct WebDropData;
struct WebPreferences;

namespace gfx {
class Point;
}

namespace webkit_glue {
class FormFieldValues;
struct WebApplicationInfo;
}

namespace WebKit {
struct WebMediaPlayerAction;
}

class URLRequestContextGetter;

//
// RenderViewHost
//
//  A RenderViewHost is responsible for creating and talking to a RenderView
//  object in a child process. It exposes a high level API to users, for things
//  like loading pages, adjusting the display and other browser functionality,
//  which it translates into IPC messages sent over the IPC channel with the
//  RenderView. It responds to all IPC messages sent by that RenderView and
//  cracks them, calling a delegate object back with higher level types where
//  possible.
//
//  The intent of this class is to provide a view-agnostic communication
//  conduit with a renderer. This is so we can build HTML views not only as
//  TabContents (see TabContents for an example) but also as views, etc.
//
//  The exact API of this object needs to be more thoroughly designed. Right
//  now it mimics what TabContents exposed, which is a fairly large API and may
//  contain things that are not relevant to a common subset of views. See also
//  the comment in render_view_host_delegate.h about the size and scope of the
//  delegate API.
//
//  Right now, the concept of page navigation (both top level and frame) exists
//  in the TabContents still, so if you instantiate one of these elsewhere, you
//  will not be able to traverse pages back and forward. We need to determine
//  if we want to bring that and other functionality down into this object so
//  it can be shared by others.
//
class RenderViewHost : public RenderWidgetHost {
 public:
  // Returns the RenderViewHost given its ID and the ID of its render process.
  // Returns NULL if the IDs do not correspond to a live RenderViewHost.
  static RenderViewHost* FromID(int render_process_id, int render_view_id);

  // routing_id could be a valid route id, or it could be MSG_ROUTING_NONE, in
  // which case RenderWidgetHost will create a new one.
  RenderViewHost(SiteInstance* instance,
                 RenderViewHostDelegate* delegate,
                 int routing_id,
                 int64 session_storage_namespace_id);
  virtual ~RenderViewHost();

  SiteInstance* site_instance() const { return instance_; }
  RenderViewHostDelegate* delegate() const { return delegate_; }

  // Set up the RenderView child process. Virtual because it is overridden by
  // TestRenderViewHost.
  virtual bool CreateRenderView(URLRequestContextGetter* request_context);

  // Returns true if the RenderView is active and has not crashed. Virtual
  // because it is overridden by TestRenderViewHost.
  virtual bool IsRenderViewLive() const;

  // Send the renderer process the current preferences supplied by the
  // RenderViewHostDelegate.
  void SyncRendererPrefs();

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const ViewMsg_Navigate_Params& message);

  // Load the specified URL, this is a shortcut for Navigate().
  void NavigateToURL(const GURL& url);

  // Loads the specified html (must be UTF8) in the main frame.  If
  // |new_navigation| is true, it simulates a navigation to |display_url|.
  // |security_info| is the security state that will be reported when the page
  // load commits.  It is useful for mocking SSL errors.  Provide an empty
  // string if no secure connection state should be simulated.
  // Note that if |new_navigation| is false, |display_url| and |security_info|
  // are not used.
  void LoadAlternateHTMLString(const std::string& html_text,
                               bool new_navigation,
                               const GURL& display_url,
                               const std::string& security_info);

  // Returns whether navigation messages are currently suspended for this
  // RenderViewHost.  Only true during a cross-site navigation, while waiting
  // for the onbeforeunload handler.
  bool are_navigations_suspended() { return navigations_suspended_; }

  // Suspends (or unsuspends) any navigation messages from being sent from this
  // RenderViewHost.  This is called when a pending RenderViewHost is created
  // for a cross-site navigation, because we must suspend any navigations until
  // we hear back from the old renderer's onbeforeunload handler.  Note that it
  // is important that only one navigation event happen after calling this
  // method with |suspend| equal to true.  If |suspend| is false and there is
  // a suspended_nav_message_, this will send the message.  This function
  // should only be called to toggle the state; callers should check
  // are_navigations_suspended() first.
  void SetNavigationsSuspended(bool suspend);

  // Causes the renderer to invoke the onbeforeunload event handler.  The
  // result will be returned via ViewMsg_ShouldClose. See also ClosePage which
  // will fire the PageUnload event.
  //
  // Set bool for_cross_site_transition when this close is just for the current
  // RenderView in the case of a cross-site transition. False means we're
  // closing the entire tab.
  void FirePageBeforeUnload(bool for_cross_site_transition);

  // Causes the renderer to close the current page, including running its
  // onunload event handler.  A ClosePage_ACK message will be sent to the
  // ResourceDispatcherHost when it is finished.
  //
  // Please see ViewMsg_ClosePage in resource_messages_internal.h for a
  // description of the parameters.
  void ClosePage(bool for_cross_site_transition,
                 int new_render_process_host_id,
                 int new_request_id);

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  void ClosePageIgnoringUnloadEvents();

  // Sets whether this RenderViewHost has an outstanding cross-site request,
  // for which another renderer will need to run an onunload event handler.
  // This is called before the first navigation event for this RenderViewHost,
  // and again after the corresponding OnCrossSiteResponse.
  void SetHasPendingCrossSiteRequest(bool has_pending_request, int request_id);

  // Returns the request_id for the pending cross-site request.
  // This is just needed in case the unload of the current page
  // hangs, in which case we need to swap to the pending RenderViewHost.
  int GetPendingRequestId();

  // Called by ResourceDispatcherHost when a response for a pending cross-site
  // request is received.  The ResourceDispatcherHost will pause the response
  // until the onunload handler of the previous renderer is run.
  void OnCrossSiteResponse(int new_render_process_host_id, int new_request_id);

  // Stops the current load.
  void Stop();

  // Reloads the current frame.
  void ReloadFrame();

  // Asks the renderer to "render" printed pages and initiate printing on our
  // behalf.
  bool PrintPages();

  // Notify renderer of success/failure of print job.
  void PrintingDone(int document_cookie, bool success);

  // Start looking for a string within the content of the page, with the
  // specified options.
  void StartFinding(int request_id,
                    const string16& search_string,
                    bool forward,
                    bool match_case,
                    bool find_next);

  // Cancel a pending find operation. If |clear_selection| is true, it will also
  // clear the selection on the focused frame.
  void StopFinding(bool clear_selection);

  // Change the zoom level of a page.
  void Zoom(PageZoom::Function function);

  // Change the encoding of the page.
  void SetPageEncoding(const std::string& encoding);

  // Reset any override encoding on the page and change back to default.
  void ResetPageEncodingToDefault();

  // Change the alternate error page URL.  An empty GURL disables the use of
  // alternate error pages.
  void SetAlternateErrorPageURL(const GURL& url);

  // Fill out a form within the page with the specified data.
  void FillForm(const FormData& form_data);

  // Fill out a password form and trigger DOM autocomplete in the case
  // of multiple matching logins.
  void FillPasswordForm(
      const webkit_glue::PasswordFormDomManager::FillData& form_data);

  // D&d drop target messages that get sent to WebKit.
  void DragTargetDragEnter(const WebDropData& drop_data,
                           const gfx::Point& client_pt,
                           const gfx::Point& screen_pt,
                           WebKit::WebDragOperationsMask operations_allowed);
  void DragTargetDragOver(const gfx::Point& client_pt,
                          const gfx::Point& screen_pt,
                          WebKit::WebDragOperationsMask operations_allowed);
  void DragTargetDragLeave();
  void DragTargetDrop(const gfx::Point& client_pt,
                      const gfx::Point& screen_pt);

  // Tell the RenderView to reserve a range of page ids of the given size.
  void ReservePageIDRange(int size);

  // Runs some javascript within the context of a frame in the page.
  void ExecuteJavascriptInWebFrame(const std::wstring& frame_xpath,
                                   const std::wstring& jscript);

  // Insert some css into a frame in the page. |id| is optional, and specifies
  // the element id given when inserting/replacing the style element.
  void InsertCSSInWebFrame(const std::wstring& frame_xpath,
                           const std::string& css,
                           const std::string& id);

  // Logs a message to the console of a frame in the page.
  void AddMessageToConsole(const string16& frame_xpath,
                           const string16& message,
                           const WebKit::WebConsoleMessage::Level&);

  // Edit operations.
  void Undo();
  void Redo();
  void Cut();
  void Copy();
  void CopyToFindPboard();
  void Paste();
  void ToggleSpellCheck();
  void Delete();
  void SelectAll();
  void ToggleSpellPanel(bool is_currently_visible);

  // Downloads an image notifying the FavIcon delegate appropriately. The
  // returned integer uniquely identifies the download for the lifetime of the
  // browser.
  int DownloadFavIcon(const GURL& url, int image_size);

  // Requests application info for the specified page. This is an asynchronous
  // request. The delegate is notified by way of OnDidGetApplicationInfo when
  // the data is available.
  void GetApplicationInfo(int32 page_id);

  // Captures a thumbnail representation of the page.
  void CaptureThumbnail();

  // Captures a snapshot of the page.
  void CaptureSnapshot();

  // Notifies the RenderView that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt);

  // Notifies the RenderView that the modal html dialog has been closed.
  void ModalHTMLDialogClosed(IPC::Message* reply_msg,
                             const std::string& json_retval);

  // Send an action to the media player element located at |location|.
  void MediaPlayerActionAt(const gfx::Point& location,
                           const WebKit::WebMediaPlayerAction& action);

  // Copies the image at the specified point.
  void CopyImageAt(int x, int y);

  // Notifies the renderer that a a drag operation that it started has ended,
  // either in a drop or by being cancelled.
  void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y,
      WebKit::WebDragOperation operation);

  // Notifies the renderer that a drag and drop operation is in progress, with
  // droppable items positioned over the renderer's view.
  void DragSourceMovedTo(
      int client_x, int client_y, int screen_x, int screen_y);

  // Notifies the renderer that we're done with the drag and drop operation.
  // This allows the renderer to reset some state.
  void DragSourceSystemDragEnded();

  // Tell the render view to enable a set of javascript bindings. The argument
  // should be a combination of values from BindingsPolicy.
  void AllowBindings(int binding_flags);

  // Sets a property with the given name and value on the DOM UI binding object.
  // Must call AllowDOMUIBindings() on this renderer first.
  void SetDOMUIProperty(const std::string& name, const std::string& value);

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  // Clears the node that is currently focused (if any).
  void ClearFocusedNode();

  // Update render view specific (WebKit) preferences.
  void UpdateWebPreferences(const WebPreferences& prefs);

  // Request the Renderer to ask the default plugin to start installation of
  // missing plugin. Called by PluginInstaller.
  void InstallMissingPlugin();

  // Get all savable resource links from current webpage, include main
  // frame and sub-frame.
  void GetAllSavableResourceLinksForCurrentPage(const GURL& page_url);

  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  // The parameter links contain original URLs of all saved links.
  // The parameter local_paths contain corresponding local file paths of
  // all saved links, which matched with vector:links one by one.
  // The parameter local_directory_name is relative path of directory which
  // contain all saved auxiliary files included all sub frames and resouces.
  void GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<FilePath>& local_paths,
      const FilePath& local_directory_name);

  // Notifies the RenderViewHost that a file has been chosen by the user from
  // an Open File dialog for the form.
  void FileSelected(const FilePath& path);

  // Notifies the Listener that many files have been chosen by the user from
  // an Open File dialog for the form.
  void MultiFilesSelected(const std::vector<FilePath>& files);

  // Notifies the RenderViewHost that its load state changed.
  void LoadStateChanged(const GURL& url, net::LoadState load_state,
                        uint64 upload_position, uint64 upload_size);

  bool SuddenTerminationAllowed() const;
  void set_sudden_termination_allowed(bool enabled) {
    sudden_termination_allowed_ = enabled;
  }

  // Forward a message from external host to chrome renderer.
  void ForwardMessageFromExternalHost(const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);

  // Message the renderer that we should be counted as a new document and not
  // as a popup.
  void DisassociateFromPopupCount();

  // Notifies the Renderer that we've either displayed or hidden the popup
  // notification.
  void PopupNotificationVisibilityChanged(bool visible);

  // Called by the FormFieldHistoryManager when the list of suggestions is
  // ready.
  void FormFieldHistorySuggestionsReturned(
      int query_id,
      const std::vector<string16>& suggestions,
      int default_suggestion_index);

  // Notifies the Renderer that a move or resize of its containing window has
  // started (this is used to hide the autocomplete popups if any).
  void WindowMoveOrResizeStarted();

  // RenderWidgetHost public overrides.
  virtual void Shutdown();
  virtual bool IsRenderView() const { return true; }
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void GotFocus();
  virtual bool CanBlur() const;
  virtual void ForwardMouseEvent(const WebKit::WebMouseEvent& mouse_event);
  virtual void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event);
  virtual void ForwardEditCommand(const std::string& name,
                                  const std::string& value);
  virtual void ForwardEditCommandsForNextKeyEvent(
      const EditCommands& edit_commands);
  virtual gfx::Rect GetRootWindowResizerRect() const;

  // Creates a new RenderView with the given route id.
  void CreateNewWindow(int route_id);

  // Creates a new RenderWidget with the given route id.
  void CreateNewWidget(int route_id, bool activatable);

  // Sends the response to an extension api call.
  void SendExtensionResponse(int request_id, bool success,
                             const std::string& response,
                             const std::string& error);

  // Sends a response to an extension api call that it was blocked for lack of
  // permission.
  void BlockExtensionRequest(int request_id);

  // Notifies the renderer that its view type has changed.
  void ViewTypeChanged(ViewType::Type type);

  // Tells the renderer which browser window it is being attached to.
  void UpdateBrowserWindowId(int window_id);

  // Tells the render view that a custom context action has been selected.
  void PerformCustomContextMenuAction(unsigned action);

  // Tells the renderer to translate the current page from one language to
  // another.  If the current page id is not |page_id|, the request is ignored.
  void TranslatePage(int page_id,
                     const std::string& source_lang,
                     const std::string& target_lang);

  // Informs renderer of updated content settings.
  void SendContentSettings(const std::string& host,
                           const ContentSettings& settings);

 protected:
  // RenderWidgetHost protected overrides.
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);
  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void OnUserGesture();
  virtual void NotifyRendererUnresponsive();
  virtual void NotifyRendererResponsive();
  virtual void OnMsgFocusedNodeChanged();

  // IPC message handlers.
  void OnMsgShowView(int route_id,
                     WindowOpenDisposition disposition,
                     const gfx::Rect& initial_pos,
                     bool user_gesture);
  void OnMsgShowWidget(int route_id, const gfx::Rect& initial_pos);
  void OnMsgRunModal(IPC::Message* reply_msg);
  void OnMsgRenderViewReady();
  void OnMsgRenderViewGone();
  void OnMsgNavigate(const IPC::Message& msg);
  void OnMsgUpdateState(int32 page_id,
                        const std::string& state);
  void OnMsgUpdateTitle(int32 page_id, const std::wstring& title);
  void OnMsgUpdateEncoding(const std::string& encoding);
  void OnMsgUpdateTargetURL(int32 page_id, const GURL& url);
  void OnMsgThumbnail(const GURL& url,
                      const ThumbnailScore& score,
                      const SkBitmap& bitmap);
  void OnMsgScreenshot(const SkBitmap& bitmap);
  void OnMsgClose();
  void OnMsgRequestMove(const gfx::Rect& pos);
  void OnMsgDidRedirectProvisionalLoad(int32 page_id,
                                       const GURL& source_url,
                                       const GURL& target_url);
  void OnMsgDidStartLoading();
  void OnMsgDidStopLoading();
  void OnMsgDocumentAvailableInMainFrame();
  void OnMsgDidLoadResourceFromMemoryCache(const GURL& url,
                                           const std::string& frame_origin,
                                           const std::string& main_frame_origin,
                                           const std::string& security_info);
  void OnMsgDidDisplayInsecureContent();
  void OnMsgDidRunInsecureContent(const std::string& security_origin);
  void OnMsgDidStartProvisionalLoadForFrame(bool main_frame,
                                            const GURL& url);
  void OnMsgDidFailProvisionalLoadWithError(bool main_frame,
                                            int error_code,
                                            const GURL& url,
                                            bool showing_repost_interstitial);
  void OnMsgFindReply(int request_id,
                      int number_of_matches,
                      const gfx::Rect& selection_rect,
                      int active_match_ordinal,
                      bool final_update);
  void OnExecuteCodeFinished(int request_id, bool success);
  void OnMsgUpdateFavIconURL(int32 page_id, const GURL& icon_url);
  void OnMsgDidDownloadFavIcon(int id,
                               const GURL& image_url,
                               bool errored,
                               const SkBitmap& image_data);
  void OnMsgContextMenu(const ContextMenuParams& params);
  void OnMsgOpenURL(const GURL& url, const GURL& referrer,
                    WindowOpenDisposition disposition);
  void OnMsgDidContentsPreferredSizeChange(const gfx::Size& new_size);
  void OnMsgDomOperationResponse(const std::string& json_string,
                                 int automation_id);
  void OnMsgDOMUISend(const std::string& message,
                      const std::string& content);
  void OnMsgForwardMessageToExternalHost(const std::string& message,
                                         const std::string& origin,
                                         const std::string& target);
  void OnMsgDocumentLoadedInFrame();
  void OnMsgGoToEntryAtOffset(int offset);
  void OnMsgSetTooltipText(const std::wstring& tooltip_text,
                           WebKit::WebTextDirection text_direction_hint);
  void OnMsgSelectionChanged(const std::string& text);
  void OnMsgPasteFromSelectionClipboard();
  void OnMsgRunFileChooser(bool multiple_files,
                           const string16& title,
                           const FilePath& default_file);
  void OnMsgRunJavaScriptMessage(const std::wstring& message,
                                 const std::wstring& default_prompt,
                                 const GURL& frame_url,
                                 const int flags,
                                 IPC::Message* reply_msg);
  void OnMsgRunBeforeUnloadConfirm(const GURL& frame_url,
                                   const std::wstring& message,
                                   IPC::Message* reply_msg);
  void OnMsgShowModalHTMLDialog(const GURL& url, int width, int height,
                                const std::string& json_arguments,
                                IPC::Message* reply_msg);
  void OnMsgPasswordFormsSeen(
      const std::vector<webkit_glue::PasswordForm>& forms);
  void OnMsgFormFieldValuesSubmitted(const webkit_glue::FormFieldValues& forms);
  void OnMsgStartDragging(const WebDropData& drop_data,
                          WebKit::WebDragOperationsMask operations_allowed);
  void OnUpdateDragCursor(WebKit::WebDragOperation drag_operation);
  void OnTakeFocus(bool reverse);
  void OnMsgPageHasOSDD(int32 page_id, const GURL& doc_url, bool autodetected);
  void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);
  void OnAddMessageToConsole(const std::wstring& message,
                             int32 line_no,
                             const std::wstring& source_id);

  void OnUpdateInspectorSettings(const std::string& raw_settings);
  void OnForwardToDevToolsAgent(const IPC::Message& message);
  void OnForwardToDevToolsClient(const IPC::Message& message);
  void OnActivateDevToolsWindow();
  void OnCloseDevToolsWindow();
  void OnDockDevToolsWindow();
  void OnUndockDevToolsWindow();
  void OnDevToolsRuntimeFeatureStateChanged(const std::string& feature,
                                            bool enabled);

  void OnUserMetricsRecordAction(const std::string& action);
  void OnMissingPluginStatus(int status);
  void OnCrashedPlugin(const FilePath& plugin_path);

  void OnReceivedSavableResourceLinksForCurrentPage(
      const std::vector<GURL>& resources_list,
      const std::vector<GURL>& referrers_list,
      const std::vector<GURL>& frames_list);

  void OnReceivedSerializedHtmlData(const GURL& frame_url,
                                    const std::string& data,
                                    int32 status);

  void OnDidGetApplicationInfo(int32 page_id,
                               const webkit_glue::WebApplicationInfo& info);
  void OnMsgShouldCloseACK(bool proceed);
  void OnQueryFormFieldAutofill(int request_id,
                                const string16& field_name,
                                const string16& user_text);
  void OnRemoveAutofillEntry(const string16& field_name,
                             const string16& value);

  void OnShowDesktopNotification(const GURL& source_origin,
                                 const GURL& url, int notification_id);
  void OnShowDesktopNotificationText(const GURL& origin, const GURL& icon,
                                     const string16& title,
                                     const string16& text,
                                     int notification_id);
  void OnCancelDesktopNotification(int notification_id);
  void OnRequestNotificationPermission(const GURL& origin, int callback_id);

  void OnExtensionRequest(const std::string& name, const ListValue& args,
                          int request_id, bool has_callback);
  void OnExtensionPostMessage(int port_id, const std::string& message);
  void OnAccessibilityFocusChange(int acc_obj_id);
  void OnCSSInserted();
  void OnPageContents(const GURL& url,
                      int32 page_id,
                      const std::wstring& contents,
                      const std::string& language);
  void OnPageTranslated(int32 page_id,
                        const std::string& original_lang,
                        const std::string& translated_lang);

 private:
  friend class TestRenderViewHost;

  void UpdateBackForwardListCount();

  // The SiteInstance associated with this RenderViewHost.  All pages drawn
  // in this RenderViewHost are part of this SiteInstance.  Should not change
  // over time.
  scoped_refptr<SiteInstance> instance_;

  // Our delegate, which wants to know about changes in the RenderView.
  RenderViewHostDelegate* delegate_;

  // true if we are currently waiting for a response for drag context
  // information.
  bool waiting_for_drag_context_response_;

  // A bitwise OR of bindings types that have been enabled for this RenderView.
  // See BindingsPolicy for details.
  int enabled_bindings_;

  // The request_id for the pending cross-site request. Set to -1 if
  // there is a pending request, but we have not yet started the unload
  // for the current page. Set to the request_id value of the pending
  // request once we have gotten the some data for the pending page
  // and thus started the unload process.
  int pending_request_id_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them.  This will be true when a RenderViewHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderViewHost.
  bool navigations_suspended_;

  // We only buffer a suspended navigation message while we a pending RVH for a
  // TabContents.  There will only ever be one suspended navigation, because
  // TabContents will destroy the pending RVH and create a new one if a second
  // navigation occurs.
  scoped_ptr<ViewMsg_Navigate> suspended_nav_message_;

  // If we were asked to RunModal, then this will hold the reply_msg that we
  // must return to the renderer to unblock it.
  IPC::Message* run_modal_reply_msg_;

  // Set to true when there is a pending ViewMsg_ShouldClose message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or is_waiting_for_unload_ack_ is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  bool is_waiting_for_beforeunload_ack_;

  // Set to true when there is a pending ViewMsg_Close message.  Also see
  // is_waiting_for_beforeunload_ack_, unload_ack_is_for_cross_site_transition_.
  bool is_waiting_for_unload_ack_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // is_waiting_for_unload_ack_ is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderViewHost in
  // the case of a cross-site transition ( = true).
  bool unload_ack_is_for_cross_site_transition_;

  bool are_javascript_messages_suppressed_;

  // True if the render view can be shut down suddenly.
  bool sudden_termination_allowed_;

  // The session storage namespace id to be used by the associated render view.
  int64 session_storage_namespace_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_H_
