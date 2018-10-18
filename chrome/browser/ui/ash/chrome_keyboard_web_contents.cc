// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_web_contents.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_resource_util.h"

namespace {

// Deletes itself when the associated WebContents is destroyed.
class ChromeKeyboardContentsDelegate : public content::WebContentsDelegate,
                                       public content::WebContentsObserver {
 public:
  ChromeKeyboardContentsDelegate() = default;
  ~ChromeKeyboardContentsDelegate() override = default;

 private:
  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    source->GetController().LoadURL(params.url, params.referrer,
                                    params.transition, params.extra_headers);
    Observe(source);
    return source;
  }

  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override {
    return false;
  }

  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override {
    return false;
  }

  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override {
    aura::Window* keyboard_window = source->GetNativeView();
    // keyboard window must have been added to keyboard container window at this
    // point. Otherwise, wrong keyboard bounds is used and may cause problem as
    // described in https://crbug.com/367788.
    DCHECK(keyboard_window->parent());
    // keyboard window bounds may not set to |pos| after this call. If keyboard
    // is in FULL_WIDTH mode, only the height of keyboard window will be
    // changed.
    keyboard_window->SetBounds(bounds);
  }

  // content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    const extensions::Extension* extension = nullptr;
    GURL origin(request.security_origin);
    if (origin.SchemeIs(extensions::kExtensionScheme)) {
      const extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext());
      extension = registry->enabled_extensions().GetByID(origin.host());
      DCHECK(extension);
    }
    MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
        web_contents, request, std::move(callback), extension);
  }

  // content::WebContentsDelegate:
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override {
    switch (event.GetType()) {
      // Scroll events are not suppressed because the menu to select IME should
      // be scrollable.
      case blink::WebInputEvent::kGestureScrollBegin:
      case blink::WebInputEvent::kGestureScrollEnd:
      case blink::WebInputEvent::kGestureScrollUpdate:
      case blink::WebInputEvent::kGestureFlingStart:
      case blink::WebInputEvent::kGestureFlingCancel:
        return false;
      default:
        // Stop gesture events from being passed to renderer to suppress the
        // context menu. https://crbug.com/685140
        return true;
    }
  }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardContentsDelegate);
};

}  // namespace

ChromeKeyboardWebContents::ChromeKeyboardWebContents(
    content::BrowserContext* context,
    const GURL& url) {
  DCHECK(context);
  content::WebContents::CreateParams web_contents_params(
      context, content::SiteInstance::CreateForURL(context, url));
  web_contents_ = content::WebContents::Create(web_contents_params);
  web_contents_->SetDelegate(new ChromeKeyboardContentsDelegate());

  extensions::SetViewType(web_contents_.get(), extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_.get());
  Observe(web_contents_.get());
  LoadContents(url);
}

ChromeKeyboardWebContents::~ChromeKeyboardWebContents() = default;

void ChromeKeyboardWebContents::SetKeyboardUrl(const GURL& new_url) {
  GURL old_url = web_contents_->GetURL();
  if (old_url == new_url)
    return;

  if (old_url.GetOrigin() != new_url.GetOrigin()) {
    // Sets keyboard window rectangle to 0 and closes the current page before
    // navigating to a keyboard in a different extension. This keeps the UX the
    // same as Android. Note we need to explicitly close the current page as it
    // might try to resize the keyboard window in javascript on a resize event.
    TRACE_EVENT0("vk", "ReloadKeyboardIfNeeded");
    web_contents_->GetNativeView()->SetBounds(gfx::Rect());
    web_contents_->ClosePage();
  }

  LoadContents(new_url);
}

void ChromeKeyboardWebContents::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  content::HostZoomMap::GetDefaultForBrowserContext(
      render_process_host->GetBrowserContext())
      ->SetTemporaryZoomLevel(render_process_host->GetID(),
                              render_view_host->GetRoutingID(), 0 /* level */);
}

void ChromeKeyboardWebContents::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // TODO(mash): Support virtual keyboard. https://crbug.com/843332.
  if (!features::IsMultiProcessMash())
    keyboard::KeyboardController::Get()->NotifyKeyboardWindowLoaded();
}

void ChromeKeyboardWebContents::LoadContents(const GURL& url) {
  TRACE_EVENT0("vk", "LoadContents");
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::SINGLETON_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  web_contents_->OpenURL(params);
}
