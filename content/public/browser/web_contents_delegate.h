// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/window_container_type.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/window_open_disposition.h"

class FilePath;
class GURL;
class TabContents;
struct ContextMenuParams;
struct NativeWebKeyboardEvent;

namespace base {
class ListValue;
}

namespace content {
class BrowserContext;
class DownloadItem;
class JavaScriptDialogCreator;
class WebContents;
class WebIntentsDispatcher;
struct FileChooserParams;
struct SSLStatus;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace history {
class HistoryAddPageArgs;
}

namespace webkit_glue {
struct WebIntentData;
}

namespace content {

struct OpenURLParams;

// Objects implement this interface to get notified about changes in the
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT WebContentsDelegate {
 public:
  WebContentsDelegate();

  // Opens a new URL inside the passed in WebContents (if source is 0 open
  // in the current front-most tab), unless |disposition| indicates the url
  // should be opened in a new tab or window.
  //
  // A NULL source indicates the current tab (callers should probably use
  // OpenURL() for these cases which does it for you).

  // Returns the WebContents the URL is opened in, or NULL if the URL wasn't
  // opened immediately.
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params);

  // Called to inform the delegate that the tab content's navigation state
  // changed. The |changed_flags| indicates the parts of the navigation state
  // that have been updated, and is any combination of the
  // |WebContents::InvalidateTypes| bits.
  virtual void NavigationStateChanged(const WebContents* source,
                                      unsigned changed_flags) {}

  // Adds the navigation request headers to |headers|. Use
  // net::HttpUtil::AppendHeaderIfMissing to build the set of headers.
  virtual void AddNavigationHeaders(const GURL& url, std::string* headers) {}

  // Creates a new tab with the already-created WebContents 'new_contents'.
  // The window for the added contents should be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |pos| should hold the
  // initial position.
  virtual void AddNewContents(WebContents* source,
                              WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}

  // Selects the specified contents, bringing its container to the front.
  virtual void ActivateContents(WebContents* contents) {}

  // Deactivates the specified contents by deactivating its container and
  // potentialy moving it to the back of the Z order.
  virtual void DeactivateContents(WebContents* contents) {}

  // Notifies the delegate that this contents is starting or is done loading
  // some resource. The delegate should use this notification to represent
  // loading feedback. See WebContents::IsLoading()
  virtual void LoadingStateChanged(WebContents* source) {}

  // Notifies the delegate that the page has made some progress loading.
  // |progress| is a value between 0.0 (nothing loaded) to 1.0 (page fully
  // loaded).
  // Note that to receive this notification, you must have called
  // SetReportLoadProgressEnabled(true) in the render view.
  virtual void LoadProgressChanged(double progress) {}

  // Request the delegate to close this web contents, and do whatever cleanup
  // it needs to do.
  virtual void CloseContents(WebContents* source) {}

  // Informs the delegate that the underlying RenderViewHost has been swapped
  // out so it can perform any cleanup necessary.
  virtual void SwappedOut(WebContents* source) {}

  // Request the delegate to move this tab contents to the specified position
  // in screen coordinates.
  virtual void MoveContents(WebContents* source, const gfx::Rect& pos) {}

  // Causes the delegate to detach |source| and clean up any internal data
  // pointing to it.  After this call ownership of |source| passes to the
  // caller, and it is safe to call "source->set_delegate(someone_else);".
  virtual void DetachContents(WebContents* source) {}

  // Called to determine if the WebContents is contained in a popup window
  // or a panel window.
  virtual bool IsPopupOrPanel(const WebContents* source) const;

  // Notification that the target URL has changed.
  virtual void UpdateTargetURL(WebContents* source,
                               int32 page_id,
                               const GURL& url) {}

  // Notification that there was a mouse event, along with the absolute
  // coordinates of the mouse pointer and whether it was a normal motion event
  // (otherwise, the pointer left the contents area).
  virtual void ContentsMouseEvent(WebContents* source,
                                  const gfx::Point& location,
                                  bool motion) {}

  // Request the delegate to change the zoom level of the current tab.
  virtual void ContentsZoomChange(bool zoom_in) {}

  // Check whether this contents is inside a window dedicated to running a web
  // application.
  virtual bool IsApplication() const;

  // Detach the given tab and convert it to a "webapp" view.  The tab must be
  // a WebContents with a valid WebApp set.
  virtual void ConvertContentsToApplication(WebContents* source) {}

  // Whether the specified tab can be reloaded.
  // Reloading can be disabled e. g. for the DevTools window.
  virtual bool CanReloadContents(WebContents* source) const;

  // Invoked prior to showing before unload handler confirmation dialog.
  virtual void WillRunBeforeUnloadConfirm() {}

  // Returns true if javascript dialogs and unload alerts are suppressed.
  // Default is false.
  virtual bool ShouldSuppressDialogs();

  // Tells us that we've finished firing this tab's beforeunload event.
  // The proceed bool tells us whether the user chose to proceed closing the
  // tab. Returns true if the tab can continue on firing it's unload event.
  // If we're closing the entire browser, then we'll want to delay firing
  // unload events until all the beforeunload events have fired.
  virtual void BeforeUnloadFired(WebContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);

  // Sets focus to the location bar or some other place that is appropriate.
  // This is called when the tab wants to encourage user input, like for the
  // new tab page.
  virtual void SetFocusToLocationBar(bool select_all) {}

  // Returns whether the page should be focused when transitioning from crashed
  // to live. Default is true.
  virtual bool ShouldFocusPageAfterCrash();

  // Called when a popup select is about to be displayed. The delegate can use
  // this to disable inactive rendering for the frame in the window the select
  // is opened within if necessary.
  virtual void RenderWidgetShowing() {}

  // This is called when WebKit tells us that it is done tabbing through
  // controls on the page. Provides a way for WebContentsDelegates to handle
  // this. Returns true if the delegate successfully handled it.
  virtual bool TakeFocus(bool reverse);

  // Invoked when the page loses mouse capture.
  virtual void LostCapture() {}

  // Notification that |contents| has gained focus.
  virtual void WebContentsFocused(WebContents* contents) {}

  // Asks the delegate if the given tab can download.
  virtual bool CanDownload(WebContents* source, int request_id);

  // Notifies the delegate that a download is starting.
  virtual void OnStartDownload(WebContents* source, DownloadItem* download) {}

  // Return much extra vertical space should be allotted to the
  // render view widget during various animations (e.g. infobar closing).
  // This is used to make painting look smoother.
  virtual int GetExtraRenderViewHeight() const;

  // Returns true if the context menu operation was handled by the delegate.
  virtual bool HandleContextMenu(const ContextMenuParams& params);

  // Returns true if the context menu command was handled
  virtual bool ExecuteContextMenuCommand(int command);

  // Shows the page info using the specified information.
  // |url| is the url of the page/frame the info applies to, |ssl| is the SSL
  // information for that page/frame.  If |show_history| is true, a section
  // showing how many times that URL has been visited is added to the page info.
  virtual void ShowPageInfo(content::BrowserContext* browser_context,
                            const GURL& url,
                            const SSLStatus& ssl,
                            bool show_history) {}

  // Opens source view for given tab contents that is navigated to the given
  // page url.
  virtual void ViewSourceForTab(WebContents* source, const GURL& page_url);

  // Opens source view for the given subframe.
  virtual void ViewSourceForFrame(WebContents* source,
                                  const GURL& url,
                                  const std::string& content_state);

  // Allows delegates to handle keyboard events before sending to the renderer.
  // Returns true if the |event| was handled. Otherwise, if the |event| would be
  // handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
  // |*is_keyboard_shortcut| should be set to true.
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);

  // Allows delegates to handle unhandled keyboard messages coming back from
  // the renderer.
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  virtual void HandleMouseDown() {}
  virtual void HandleMouseUp() {}
  virtual void HandleMouseActivate() {}

  // Render view drag n drop ended.
  virtual void DragEnded() {}

  // Shows the repost form confirmation dialog box.
  virtual void ShowRepostFormWarningDialog(WebContents* source) {}

  // Allows delegate to override navigation to the history entries.
  // Returns true to allow WebContents to continue with the default processing.
  virtual bool OnGoToEntryOffset(int offset);

  // Returns whether this tab contents should add the specified navigation to
  // history.
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType navigation_type);

  // Returns the native window framing the view containing the tab contents.
  virtual gfx::NativeWindow GetFrameNativeWindow();

  // Allows delegate to control whether a WebContents will be created. Returns
  // true to allow the creation. Default is to allow it.
  virtual bool ShouldCreateWebContents(
      WebContents* web_contents,
      int route_id,
      WindowContainerType window_container_type,
      const string16& frame_name);

  // Notifies the delegate about the creation of a new WebContents. This
  // typically happens when popups are created.
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int64 source_frame_id,
                                  const GURL& target_url,
                                  WebContents* new_contents) {}

  // Notifies the delegate that the content restrictions for this tab has
  // changed.
  virtual void ContentRestrictionsChanged(WebContents* source) {}

  // Notification that the tab is hung.
  virtual void RendererUnresponsive(WebContents* source) {}

  // Notification that the tab is no longer hung.
  virtual void RendererResponsive(WebContents* source) {}

  // Notification that a worker associated with this tab has crashed.
  virtual void WorkerCrashed(WebContents* source) {}

  // Invoked when a main fram navigation occurs.
  virtual void DidNavigateMainFramePostCommit(WebContents* tab) {}

  // Invoked when navigating to a pending entry. When invoked the
  // NavigationController has configured its pending entry, but it has not yet
  // been committed.
  virtual void DidNavigateToPendingEntry(WebContents* tab) {}

  // Returns a pointer to a service to create JavaScript dialogs. The default
  // pointer returned is to a stub service that marks all dialogs as suppressed
  // and displays nothing.
  virtual JavaScriptDialogCreator* GetJavaScriptDialogCreator();

  // Called when a file selection is to be done.
  virtual void RunFileChooser(WebContents* tab,
                              const FileChooserParams& params) {}

  // Request to enumerate a directory.  This is equivalent to running the file
  // chooser in directory-enumeration mode and having the user select the given
  // directory.
  virtual void EnumerateDirectory(WebContents* tab,
                                  int request_id,
                                  const FilePath& path) {}

  // Called when the renderer puts a tab into or out of fullscreen mode.
  virtual void ToggleFullscreenModeForTab(WebContents* tab,
                                          bool enter_fullscreen) {}
  virtual bool IsFullscreenForTab(const WebContents* tab) const;

  // Called when a Javascript out of memory notification is received.
  virtual void JSOutOfMemory(WebContents* tab) {}

  // Register a new handler for URL requests with the given scheme.
  virtual void RegisterProtocolHandler(WebContents* tab,
                                       const std::string& protocol,
                                       const GURL& url,
                                       const string16& title) {}

  // Register a new handler for Intents with the given action and type filter.
  virtual void RegisterIntentHandler(WebContents* tab,
                                     const string16& action,
                                     const string16& type,
                                     const string16& href,
                                     const string16& title,
                                     const string16& disposition) {}

  // Web Intents notification handler. Takes ownership of the
  // |intents_dispatcher|.
  virtual void WebIntentDispatch(WebContents* tab,
                                 WebIntentsDispatcher* intents_dispatcher);

  // Result of string search in the page. This includes the number of matches
  // found and the selection rect (in screen coordinates) for the string found.
  // If |final_update| is false, it indicates that more results follow.
  virtual void FindReply(WebContents* tab,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) {}

  // Notification that a plugin has crashed.
  virtual void CrashedPlugin(WebContents* tab, const FilePath& plugin_path) {}

  // Invoked when the preferred size of the contents has been changed.
  virtual void UpdatePreferredSize(WebContents* tab,
                                   const gfx::Size& pref_size) {}

  // Notification message from HTML UI.
  virtual void WebUISend(WebContents* tab,
                         const GURL& source_url,
                         const std::string& name,
                         const base::ListValue& args) {}

  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting tab
  // contents.
  virtual void RequestToLockMouse(WebContents* tab) {}

  // Notification that the page has lost the mouse lock.
  virtual void LostMouseLock() {}

 protected:
  virtual ~WebContentsDelegate();

 private:
  friend class ::TabContents;

  // Called when |this| becomes the WebContentsDelegate for |source|.
  void Attach(WebContents* source);

  // Called when |this| is no longer the WebContentsDelegate for |source|.
  void Detach(WebContents* source);

  // The WebContents that this is currently a delegate for.
  std::set<WebContents*> attached_contents_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_DELEGATE_H_
