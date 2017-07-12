// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message_macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace virtual_keyboard_private = extensions::api::virtual_keyboard_private;

namespace {

class AshKeyboardControllerObserver
    : public keyboard::KeyboardControllerObserver {
 public:
  explicit AshKeyboardControllerObserver(content::BrowserContext* context)
      : context_(context) {}
  ~AshKeyboardControllerObserver() override {}

  // KeyboardControllerObserver overrides:
  void OnKeyboardBoundsChanging(const gfx::Rect& bounds) override {
    extensions::EventRouter* router = extensions::EventRouter::Get(context_);

    if (!router->HasEventListener(
            virtual_keyboard_private::OnBoundsChanged::kEventName)) {
      return;
    }

    std::unique_ptr<base::ListValue> event_args(new base::ListValue());
    std::unique_ptr<base::DictionaryValue> new_bounds(
        new base::DictionaryValue());
    new_bounds->SetInteger("left", bounds.x());
    new_bounds->SetInteger("top", bounds.y());
    new_bounds->SetInteger("width", bounds.width());
    new_bounds->SetInteger("height", bounds.height());
    event_args->Append(std::move(new_bounds));

    auto event = base::MakeUnique<extensions::Event>(
        extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_BOUNDS_CHANGED,
        virtual_keyboard_private::OnBoundsChanged::kEventName,
        std::move(event_args), context_);
    router->BroadcastEvent(std::move(event));
  }

  void OnKeyboardClosed() override {
    extensions::EventRouter* router = extensions::EventRouter::Get(context_);

    if (!router->HasEventListener(
            virtual_keyboard_private::OnKeyboardClosed::kEventName)) {
      return;
    }

    auto event = base::MakeUnique<extensions::Event>(
        extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CLOSED,
        virtual_keyboard_private::OnKeyboardClosed::kEventName,
        base::MakeUnique<base::ListValue>(), context_);
    router->BroadcastEvent(std::move(event));
  }

 private:
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardControllerObserver);
};

}  // namespace

ChromeKeyboardUI::ChromeKeyboardUI(content::BrowserContext* context)
    : keyboard::KeyboardUIContent(context) {}

ChromeKeyboardUI::~ChromeKeyboardUI() {
  DCHECK(!keyboard_controller());
}

ui::InputMethod* ChromeKeyboardUI::GetInputMethod() {
  aura::Window* root_window = ash::Shell::GetRootWindowForNewWindows();
  DCHECK(root_window);
  return root_window->GetHost()->GetInputMethod();
}

void ChromeKeyboardUI::RequestAudioInput(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  const extensions::Extension* extension = NULL;
  GURL origin(request.security_origin);
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser_context());
    extension = registry->enabled_extensions().GetByID(origin.host());
    DCHECK(extension);
  }

  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

void ChromeKeyboardUI::SetupWebContents(content::WebContents* contents) {
  extensions::SetViewType(contents, extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
  Observe(contents);
}

void ChromeKeyboardUI::SetController(keyboard::KeyboardController* controller) {
  // During KeyboardController destruction, controller can be set to null.
  if (!controller) {
    DCHECK(keyboard_controller());
    keyboard_controller()->RemoveObserver(observer_.get());
    KeyboardUIContent::SetController(nullptr);
    return;
  }
  KeyboardUIContent::SetController(controller);
  observer_.reset(new AshKeyboardControllerObserver(browser_context()));
  keyboard_controller()->AddObserver(observer_.get());
}

void ChromeKeyboardUI::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(browser_context());
  DCHECK(zoom_map);
  int render_process_id = render_view_host->GetProcess()->GetID();
  int render_view_id = render_view_host->GetRoutingID();
  zoom_map->SetTemporaryZoomLevel(render_process_id, render_view_id, 0);
}

void ChromeKeyboardUI::ShowKeyboardContainer(aura::Window* container) {
  KeyboardUIContent::ShowKeyboardContainer(container);
}

bool ChromeKeyboardUI::ShouldWindowOverscroll(aura::Window* window) const {
  aura::Window* root_window = window->GetRootWindow();
  if (!root_window)
    return true;

  if (root_window != GetKeyboardRootWindow())
    return false;

  ash::RootWindowController* root_window_controller =
      ash::RootWindowController::ForWindow(root_window);
  // Shell ime window container contains virtual keyboard windows and IME
  // windows(IME windows are created by chrome.app.window.create api). They
  // should never be overscrolled.
  return !root_window_controller
              ->GetContainer(ash::kShellWindowId_ImeWindowParentContainer)
              ->Contains(window);
}
