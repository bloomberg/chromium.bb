// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/offscreen_presentation.h"

#include <algorithm>

#include "base/bind.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"

#if defined(USE_AURA)
#include "base/memory/weak_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#endif

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::OffscreenPresentationsOwner);

namespace {

// Upper limit on the number of simultaneous off-screen presentations per
// extension instance.
const int kMaxPresentationsPerExtension = 3;

// Time intervals used by the logic that detects when the capture of a
// presentation has stopped, to automatically tear it down and free resources.
const int kMaxSecondsToWaitForCapture = 60;
const int kPollIntervalInSeconds = 1;

#if defined(USE_AURA)

// A WindowObserver that automatically finds a root Window to adopt the
// WebContents native view containing the OffscreenPresentation content.  This
// is a workaround for Aura, which requires the WebContents native view be
// attached somewhere in the window tree in order to gain access to the
// compositing and capture functionality.  The WebContents native view, although
// attached to the window tree, will never become visible on-screen.
class WindowAdoptionAgent : protected aura::WindowObserver {
 public:
  static void Start(aura::Window* offscreen_window) {
    new WindowAdoptionAgent(offscreen_window);
    // WindowAdoptionAgent destroys itself when the Window calls
    // OnWindowDestroyed().
  }

 protected:
  void OnWindowParentChanged(aura::Window* target,
                             aura::Window* parent) final {
    if (target != offscreen_window_ || parent != nullptr)
      return;

    // Post a task to return to the event loop before finding a new parent, to
    // avoid clashing with the currently-in-progress window tree hierarchy
    // changes.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&WindowAdoptionAgent::FindNewParent,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWindowDestroyed(aura::Window* window) final {
    delete this;
  }

 private:
  explicit WindowAdoptionAgent(aura::Window* offscreen_window)
      : offscreen_window_(offscreen_window),
        weak_ptr_factory_(this) {
    DVLOG(2) << "WindowAdoptionAgent for offscreen window " << offscreen_window_
             << " is being created.";
    offscreen_window->AddObserver(this);
    OnWindowParentChanged(offscreen_window_, offscreen_window_->parent());
  }

  ~WindowAdoptionAgent() final {
    DVLOG(2) << "WindowAdoptionAgent for offscreen window " << offscreen_window_
             << " is self-destructing.";
  }

  void FindNewParent() {
    BrowserList* const browsers =
        BrowserList::GetInstance(chrome::GetActiveDesktop());
    Browser* const active_browser =
        browsers ? browsers->GetLastActive() : nullptr;
    BrowserWindow* const active_window =
        active_browser ? active_browser->window() : nullptr;
    aura::Window* const native_window =
        active_window ? active_window->GetNativeWindow() : nullptr;
    aura::Window* const root_window =
        native_window ? native_window->GetRootWindow() : nullptr;
    if (root_window) {
      DVLOG(2) << "Root window " << root_window
               << " adopts the offscreen window " << offscreen_window_ << '.';
      root_window->AddChild(offscreen_window_);
    } else {
      LOG(DFATAL) << "Unable to find an aura root window.  "
                     "OffscreenPresentation compositing may be halted!";
    }
  }

  aura::Window* const offscreen_window_;
  base::WeakPtrFactory<WindowAdoptionAgent> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowAdoptionAgent);
};

#endif  // USE_AURA

}  // namespace

namespace extensions {

OffscreenPresentationsOwner::OffscreenPresentationsOwner(WebContents* contents)
    : extension_web_contents_(contents) {
  DCHECK(extension_web_contents_);
}

OffscreenPresentationsOwner::~OffscreenPresentationsOwner() {}

// static
OffscreenPresentationsOwner* OffscreenPresentationsOwner::Get(
    content::WebContents* extension_web_contents) {
  // CreateForWebContents() really means "create if not exists."
  CreateForWebContents(extension_web_contents);
  return FromWebContents(extension_web_contents);
}

OffscreenPresentation* OffscreenPresentationsOwner::StartPresentation(
    const GURL& start_url,
    const std::string& presentation_id,
    const gfx::Size& initial_size) {
  if (presentations_.size() >= kMaxPresentationsPerExtension)
    return nullptr;  // Maximum number of presentations reached.

  presentations_.push_back(
      new OffscreenPresentation(this, start_url, presentation_id));
  presentations_.back()->Start(initial_size);
  return presentations_.back();
}

void OffscreenPresentationsOwner::ClosePresentation(
    OffscreenPresentation* presentation) {
  const auto it =
      std::find(presentations_.begin(), presentations_.end(), presentation);
  if (it != presentations_.end())
    presentations_.erase(it);
}

OffscreenPresentation::OffscreenPresentation(OffscreenPresentationsOwner* owner,
                                             const GURL& start_url,
                                             const std::string& id)
    : owner_(owner),
      start_url_(start_url),
      presentation_id_(id),
      profile_(Profile::FromBrowserContext(
                   owner->extension_web_contents()->GetBrowserContext())
               ->CreateOffTheRecordProfile()),
      capture_poll_timer_(false, false),
      content_capture_was_detected_(false) {
  DCHECK(profile_);
}

OffscreenPresentation::~OffscreenPresentation() {
  DVLOG(1) << "Destroying OffscreenPresentation for start_url="
           << start_url_.spec();
}

void OffscreenPresentation::Start(const gfx::Size& initial_size) {
  DCHECK(start_time_.is_null());
  DVLOG(1) << "Starting OffscreenPresentation with initial size of "
           << initial_size.ToString() << " for start_url=" << start_url_.spec();

  // Create the WebContents to contain the off-screen presentation page.
  presentation_web_contents_.reset(
      WebContents::Create(WebContents::CreateParams(profile_.get())));
  presentation_web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(presentation_web_contents_.get());

#if defined(USE_AURA)
  WindowAdoptionAgent::Start(presentation_web_contents_->GetNativeView());
#endif

  // Set initial size, if specified.
  if (!initial_size.IsEmpty())
    ResizeWebContents(presentation_web_contents_.get(), initial_size);

  // Mute audio output.  When tab capture starts, the audio will be
  // automatically unmuted, but will be captured into the MediaStream.
  presentation_web_contents_->SetAudioMuted(true);

  // TODO(imcheng): If |presentation_id_| is not empty, register it with the
  // PresentationRouter.  http://crbug.com/513859
  if (!presentation_id_.empty()) {
    NOTIMPLEMENTED()
        << "Register with PresentationRouter, id=" << presentation_id_;
  }

  // Navigate to the initial URL of the presentation.
  content::NavigationController::LoadURLParams load_params(start_url_);
  load_params.should_replace_current_entry = true;
  load_params.should_clear_history_list = true;
  presentation_web_contents_->GetController().LoadURLWithParams(load_params);

  start_time_ = base::TimeTicks::Now();
  DieIfContentCaptureEnded();
}

void OffscreenPresentation::CloseContents(WebContents* source) {
  DCHECK_EQ(presentation_web_contents_.get(), source);
  // Javascript in the page called window.close().
  DVLOG(1) << "OffscreenPresentation will die at renderer's request for "
              "start_url=" << start_url_.spec();
  owner_->ClosePresentation(this);
}

bool OffscreenPresentation::ShouldSuppressDialogs(WebContents* source) {
  DCHECK_EQ(presentation_web_contents_.get(), source);
  // Suppress all because there is no possible direct user interaction with
  // dialogs.
  return true;
}

bool OffscreenPresentation::ShouldFocusLocationBarByDefault(
    WebContents* source) {
  DCHECK_EQ(presentation_web_contents_.get(), source);
  // Indicate the location bar should be focused instead of the page, even
  // though there is no location bar.  This will prevent the page from
  // automatically receiving input focus, which should never occur since there
  // is not supposed to be any direct user interaction.
  return true;
}

bool OffscreenPresentation::ShouldFocusPageAfterCrash() {
  // Never focus the page.  Not even after a crash.
  return false;
}

void OffscreenPresentation::CanDownload(
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  // Presentation pages are not allowed to download files.
  callback.Run(false);
}

bool OffscreenPresentation::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Context menus should never be shown.  Do nothing, but indicate the context
  // menu was shown so that default implementation in libcontent does not
  // attempt to do so on its own.
  return true;
}

bool OffscreenPresentation::PreHandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  DCHECK_EQ(presentation_web_contents_.get(), source);
  // Intercept and silence all keyboard events before they can be sent to the
  // renderer.
  *is_keyboard_shortcut = false;
  return true;
}

bool OffscreenPresentation::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  DCHECK_EQ(presentation_web_contents_.get(), source);
  // Intercept and silence all gesture events before they can be sent to the
  // renderer.
  return true;
}

bool OffscreenPresentation::CanDragEnter(
    WebContents* source,
    const content::DropData& data,
    blink::WebDragOperationsMask operations_allowed) {
  DCHECK_EQ(presentation_web_contents_.get(), source);
  // Halt all drag attempts onto the page since there should be no direct user
  // interaction with it.
  return false;
}

bool OffscreenPresentation::ShouldCreateWebContents(
    WebContents* contents,
    int route_id,
    int main_frame_route_id,
    WindowContainerType window_container_type,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK_EQ(presentation_web_contents_.get(), contents);
  // Disallow creating separate WebContentses.  The WebContents implementation
  // uses this to spawn new windows/tabs, which is also not allowed for
  // presentation pages.
  return false;
}

bool OffscreenPresentation::EmbedsFullscreenWidget() const {
  // OffscreenPresentation will manage fullscreen widgets.
  return true;
}

void OffscreenPresentation::EnterFullscreenModeForTab(WebContents* contents,
                                                      const GURL& origin) {
  DCHECK_EQ(presentation_web_contents_.get(), contents);

  if (in_fullscreen_mode())
    return;

  // TODO(miu): Refine fullscreen handling behavior once the Presentation API
  // spec group defines this behavior.

  non_fullscreen_size_ =
      contents->GetRenderWidgetHostView()->GetViewBounds().size();
  if (contents->GetCapturerCount() >= 0 &&
      !contents->GetPreferredSize().IsEmpty()) {
    ResizeWebContents(contents, contents->GetPreferredSize());
  }
}

void OffscreenPresentation::ExitFullscreenModeForTab(WebContents* contents) {
  DCHECK_EQ(presentation_web_contents_.get(), contents);

  if (!in_fullscreen_mode())
    return;

  ResizeWebContents(contents, non_fullscreen_size_);
  non_fullscreen_size_ = gfx::Size();
}

bool OffscreenPresentation::IsFullscreenForTabOrPending(
    const WebContents* contents) const {
  DCHECK_EQ(presentation_web_contents_.get(), contents);
  return in_fullscreen_mode();
}

blink::WebDisplayMode OffscreenPresentation::GetDisplayMode(
    const WebContents* contents) const {
  DCHECK_EQ(presentation_web_contents_.get(), contents);
  return in_fullscreen_mode() ?
      blink::WebDisplayModeFullscreen : blink::WebDisplayModeBrowser;
}

void OffscreenPresentation::RequestMediaAccessPermission(
      WebContents* contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) {
  DCHECK_EQ(presentation_web_contents_.get(), contents);

  // This method is being called to check whether an extension is permitted to
  // capture the page.  Verify that the request is being made by the extension
  // that spawned this OffscreenPresentation.

  // Find the extension ID associated with the extension background page's
  // WebContents.
  content::BrowserContext* const extension_browser_context =
      owner_->extension_web_contents()->GetBrowserContext();
  const extensions::Extension* const extension =
      ProcessManager::Get(extension_browser_context)->
          GetExtensionForWebContents(owner_->extension_web_contents());
  const std::string extension_id = extension ? extension->id() : "";
  LOG_IF(DFATAL, extension_id.empty())
      << "Extension that started this OffscreenPresentation was not found.";

  // If verified, allow any tab capture audio/video devices that were requested.
  extensions::TabCaptureRegistry* const tab_capture_registry =
      extensions::TabCaptureRegistry::Get(extension_browser_context);
  content::MediaStreamDevices devices;
  if (tab_capture_registry && tab_capture_registry->VerifyRequest(
          request.render_process_id,
          request.render_frame_id,
          extension_id)) {
    if (request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_TAB_AUDIO_CAPTURE, std::string(), std::string()));
    }
    if (request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_TAB_VIDEO_CAPTURE, std::string(), std::string()));
    }
  }

  DVLOG(2) << "Allowing " << devices.size()
           << " capture devices for OffscreenPresentation content.";

  callback.Run(
    devices,
    devices.empty() ? content::MEDIA_DEVICE_INVALID_STATE :
                      content::MEDIA_DEVICE_OK,
    scoped_ptr<content::MediaStreamUI>(nullptr));
}

bool OffscreenPresentation::CheckMediaAccessPermission(
    WebContents* contents,
    const GURL& security_origin,
    content::MediaStreamType type) {
  DCHECK_EQ(presentation_web_contents_.get(), contents);
  return type == content::MEDIA_TAB_AUDIO_CAPTURE ||
      type == content::MEDIA_TAB_VIDEO_CAPTURE;
}

void OffscreenPresentation::DidShowFullscreenWidget(int routing_id) {
  if (presentation_web_contents_->GetCapturerCount() == 0 ||
      presentation_web_contents_->GetPreferredSize().IsEmpty())
    return;  // Do nothing, since no preferred size is specified.
  content::RenderWidgetHostView* const current_fs_view =
      presentation_web_contents_->GetFullscreenRenderWidgetHostView();
  if (current_fs_view)
    current_fs_view->SetSize(presentation_web_contents_->GetPreferredSize());
}

void OffscreenPresentation::DieIfContentCaptureEnded() {
  DCHECK(presentation_web_contents_.get());

  if (content_capture_was_detected_) {
    if (presentation_web_contents_->GetCapturerCount() == 0) {
      DVLOG(2) << "Capture of OffscreenPresentation content has stopped for "
                  "start_url=" << start_url_.spec();
      owner_->ClosePresentation(this);
      return;
    } else {
      DVLOG(3) << "Capture of OffscreenPresentation content continues for "
                  "start_url=" << start_url_.spec();
    }
  } else if (presentation_web_contents_->GetCapturerCount() > 0) {
    DVLOG(2) << "Capture of OffscreenPresentation content has started for "
                "start_url=" << start_url_.spec();
    content_capture_was_detected_ = true;
  } else if (base::TimeTicks::Now() - start_time_ >
                 base::TimeDelta::FromSeconds(kMaxSecondsToWaitForCapture)) {
    // More than a minute has elapsed since this OffscreenPresentation was
    // started and content capture still hasn't started.  As a safety
    // precaution, assume that content capture is never going to start and die
    // to free up resources.
    LOG(WARNING) << "Capture of OffscreenPresentation content did not start "
                    "within timeout for start_url=" << start_url_.spec();
    owner_->ClosePresentation(this);
    return;
  }

  // Schedule the timer to check again in a second.
  capture_poll_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kPollIntervalInSeconds),
      base::Bind(&OffscreenPresentation::DieIfContentCaptureEnded,
                 base::Unretained(this)));
}

}  // namespace extensions
