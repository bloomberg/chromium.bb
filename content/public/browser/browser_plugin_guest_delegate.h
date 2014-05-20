// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

namespace content {

class ColorChooser;
class JavaScriptDialogManager;
class WebContents;
struct ColorSuggestion;
struct ContextMenuParams;
struct FileChooserParams;
struct NativeWebKeyboardEvent;
struct OpenURLParams;

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() {}

  // Add a message to the console.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void AddMessageToConsole(int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) {}

  // Request the delegate to close this guest, and do whatever cleanup it needs
  // to do.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void Close() {}

  // Notification that the embedder has completed attachment.
  virtual void DidAttach() {}

  // Informs the delegate that the guest render process is gone. |status|
  // indicates whether the guest was killed, crashed, or was terminated
  // gracefully.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void GuestProcessGone(base::TerminationStatus status) {}

  // Informs the delegate that the embedder has been destroyed.
  virtual void EmbedderDestroyed() {}

  // Informs the delegate of a reply to the find request specified by
  // |request_id|.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void FindReply(int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) {}

  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Requests setting the zoom level to the provided |zoom_level|.
  virtual void SetZoom(double zoom_factor) {}

  virtual bool IsDragAndDropEnabled();

  // Notification that the page has made some progress loading. |progress| is a
  // value between 0.0 (nothing loaded) and 1.0 (page loaded completely).
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void LoadProgressed(double progress) {}

  // Notification that the guest is no longer hung.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void RendererResponsive() {}

  // Notification that the guest is hung.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void RendererUnresponsive() {}

  // Notifies that the content size of the guest has changed in autosize mode.
  virtual void SizeChanged(const gfx::Size& old_size,
                           const gfx::Size& new_size) {}

  // Asks permission to use the camera and/or microphone. If permission is
  // granted, a call should be made to |callback| with the devices. If the
  // request is denied, a call should be made to |callback| with an empty list
  // of devices. |request| has the details of the request (e.g. which of audio
  // and/or video devices are requested, and lists of available devices).
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback);

  // Asks the delegate if the given guest can download.
  // Invoking the |callback| synchronously is OK.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void CanDownload(const std::string& request_method,
                           const GURL& url,
                           const base::Callback<void(bool)>& callback);

  // Asks the delegate if the given guest can lock the pointer.
  // Invoking the |callback| synchronously is OK.
  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) {}

  // Returns a pointer to a service to manage JavaScript dialogs. May return
  // NULL in which case dialogs aren't shown.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager();

  // Called when color chooser should open. Returns the opened color chooser.
  // Returns NULL if we failed to open the color chooser (e.g. when there is a
  // ColorChooserDialog already open on Windows). Ownership of the returned
  // pointer is transferred to the caller.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual ColorChooser* OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<ColorSuggestion>& suggestions);

  // Called when a file selection is to be done.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void RunFileChooser(WebContents* web_contents,
                              const FileChooserParams& params) {}

  // Returns true if the context menu operation was handled by the delegate.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual bool HandleContextMenu(const ContextMenuParams& params);

  // Request navigating the guest to the provided |src| URL.
  virtual void NavigateGuest(const std::string& src) {}

  // Requests that the delegate destroy itself along with its associated
  // WebContents.
  virtual void Destroy() {}

  // Creates a new tab with the already-created WebContents |new_contents|.
  // The window for the added contents should be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |initial_pos| should hold
  // the initial position. If |was_blocked| is non-NULL, then |*was_blocked|
  // will be set to true if the popup gets blocked, and left unchanged
  // otherwise.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void AddNewContents(WebContents* source,
                              WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) {}

  // Opens a new URL inside the passed in WebContents (if source is 0 open
  // in the current front-most tab), unless |disposition| indicates the url
  // should be opened in a new tab or window.
  //
  // A NULL source indicates the current tab (callers should probably use
  // OpenURL() for these cases which does it for you).
  //
  // Returns the WebContents the URL is opened in, or NULL if the URL wasn't
  // opened immediately.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params);

  // Notifies the delegate about the creation of a new WebContents. This
  // typically happens when popups are created.
  // TODO(fsamuel): Delete this once BrowserPluginGuest is no longer a
  // WebContentsDelegate.
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int opener_render_frame_id,
                                  const base::string16& frame_name,
                                  const GURL& target_url,
                                  WebContents* new_contents) {}

  // Registers a |callback| with the delegate that the delegate would call when
  // it is about to be destroyed.
  typedef base::Callback<void(WebContents*)> DestructionCallback;
  virtual void RegisterDestructionCallback(
      const DestructionCallback& callback) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
