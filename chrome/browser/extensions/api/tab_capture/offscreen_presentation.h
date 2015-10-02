// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_OFFSCREEN_PRESENTATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_OFFSCREEN_PRESENTATION_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace extensions {

// TODO(miu): This file and classes should be renamed, as this implementation
// has expanded in scope to be used for all off-screen tabs.

class OffscreenPresentation;  // Forward declaration.  See below.

// Creates, owns, and manages all OffscreenPresentation instances created by the
// same extension background page.  When the extension background page's
// WebContents is about to be destroyed, its associated
// OffscreenPresentationsOwner and all of its OffscreenPresentation instances
// are destroyed.
//
// Usage:
//
//   OffscreenPresentationsOwner::Get(extension_contents)
//       ->StartPresentation(start_url, presentation_id, size);
//
// This class operates exclusively on the UI thread and so is not thread-safe.
class OffscreenPresentationsOwner
    : protected content::WebContentsUserData<OffscreenPresentationsOwner> {
 public:
  ~OffscreenPresentationsOwner() override;

  // Returns the OffscreenPresentationsOwner instance associated with the given
  // extension background page's WebContents.  Never returns nullptr.
  static OffscreenPresentationsOwner* Get(
      content::WebContents* extension_web_contents);

  // Instantiate a new off-screen tab, navigate it to |start_url|, and register
  // it with the presentation router using |presentation_id| (if a non-empty
  // string).  The new tab's main frame will start out with the given
  // |initial_size| in DIP coordinates.  If too many off-screen tabs have
  // already been started, this method returns nullptr.
  OffscreenPresentation* StartPresentation(
      const GURL& start_url,
      const std::string& optional_presentation_id,
      const gfx::Size& initial_size);

 protected:
  friend class OffscreenPresentation;

  // Accessor to the extension background page's WebContents.
  content::WebContents* extension_web_contents() const {
    return extension_web_contents_;
  }

  // Shuts down and destroys the |presentation|.
  void ClosePresentation(OffscreenPresentation* presentation);

 private:
  friend class content::WebContentsUserData<OffscreenPresentationsOwner>;

  explicit OffscreenPresentationsOwner(content::WebContents* contents);

  content::WebContents* const extension_web_contents_;
  ScopedVector<OffscreenPresentation> presentations_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenPresentationsOwner);
};

// Owns and controls a WebContents instance containing a presentation page.
// Since the presentation page does not interact with the user in any direct
// way, the WebContents is not attached to any Browser window/UI, and all input
// and focusing capabilities are blocked.
//
// OffscreenPresentation is instantiated by OffscreenPresentationsOwner.  An
// instance is shut down one of three ways:
//
//   1. When its WebContents::GetCapturerCount() returns to zero, indicating
//      there are no more consumers of its captured content (e.g., when all
//      MediaStreams have been closed).  OffscreenPresentation will detect this
//      case and initiate shutdown.
//   2. By the renderer, where the WebContents implementation will invoke the
//      WebContentsDelegate::CloseContents() override.  This occurs, for
//      example, when a page calls window.close().
//   3. Automatically, when the extension background page's WebContents is
//      destroyed.
//
// This class operates exclusively on the UI thread and so is not thread-safe.
class OffscreenPresentation : protected content::WebContentsDelegate,
                              protected content::WebContentsObserver {
 public:
  ~OffscreenPresentation() final;

  const GURL& start_url() const { return start_url_; }
  const std::string& presentation_id() const { return presentation_id_; }

  // The presentation page's WebContents instance.
  content::WebContents* web_contents() const {
    return presentation_web_contents_.get();
  }

 protected:
  friend class OffscreenPresentationsOwner;

  OffscreenPresentation(OffscreenPresentationsOwner* owner,
                        const GURL& start_url,
                        const std::string& id);

  // Creates the WebContents instance containing the presentation page,
  // configures it for off-screen rendering at the given |initial_size|, and
  // navigates it to |start_url_|.  This is invoked once by
  // OffscreenPresentationsOwner just after construction.
  void Start(const gfx::Size& initial_size);

  // content::WebContentsDelegate overrides to provide the desired behaviors.
  void CloseContents(content::WebContents* source) final;
  bool ShouldSuppressDialogs(content::WebContents* source) final;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) final;
  bool ShouldFocusPageAfterCrash() final;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback) final;
  bool HandleContextMenu(const content::ContextMenuParams& params) final;
  bool PreHandleKeyboardEvent(content::WebContents* source,
                              const content::NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) final;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) final;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) final;
  bool ShouldCreateWebContents(
      content::WebContents* contents,
      int route_id,
      int main_frame_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) final;
  bool EmbedsFullscreenWidget() const final;
  void EnterFullscreenModeForTab(content::WebContents* contents,
                                 const GURL& origin) final;
  void ExitFullscreenModeForTab(content::WebContents* contents) final;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* contents) const final;
  blink::WebDisplayMode GetDisplayMode(
      const content::WebContents* contents) const final;
  void RequestMediaAccessPermission(
      content::WebContents* contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) final;
  bool CheckMediaAccessPermission(content::WebContents* contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) final;

  // content::WebContentsObserver overrides
  void DidShowFullscreenWidget(int routing_id) final;

 private:
  bool in_fullscreen_mode() const {
    return !non_fullscreen_size_.IsEmpty();
  }

  // Called by |capture_poll_timer_| to automatically destroy this
  // OffscreenPresentation when the capturer count returns to zero.
  void DieIfContentCaptureEnded();

  OffscreenPresentationsOwner* const owner_;

  // The starting URL for this presentation, which may or may not match the
  // current WebContents URL if navigations have occurred.
  const GURL start_url_;

  // The presentation ID used to help uniquely identify this instance.
  const std::string presentation_id_;

  // A non-shared off-the-record profile based on the profile of the extension
  // background page.
  const scoped_ptr<Profile> profile_;

  // The WebContents containing the offscreen presentation page.
  scoped_ptr<content::WebContents> presentation_web_contents_;

  // The time at which Start() finished creating |presentation_web_contents_|.
  base::TimeTicks start_time_;

  // Set to the original size of the renderer just before entering fullscreen
  // mode.  When not in fullscreen mode, this is an empty size.
  gfx::Size non_fullscreen_size_;

  // Poll timer to monitor the capturer count on |presentation_web_contents_|.
  // When the capturer count returns to zero, this OffscreenPresentation is
  // automatically destroyed.
  // TODO(miu): Add a method to WebContentsObserver to report capturer count
  // changes and get rid of this polling-based approach.
  base::Timer capture_poll_timer_;

  // This is false until after the Start() method is called, and capture of the
  // |presentation_web_contents_| is first detected.
  bool content_capture_was_detected_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenPresentation);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_OFFSCREEN_PRESENTATION_H_
