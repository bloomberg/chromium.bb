// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/macros.h"
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

typedef virtual_keyboard_private::OnTextInputBoxFocused::Context Context;

namespace {

const char* kVirtualKeyboardExtensionID = "mppnpdlheglhdfmldimlhpnegondlapf";

virtual_keyboard_private::OnTextInputBoxFocusedType
TextInputTypeToGeneratedInputTypeEnum(ui::TextInputType type) {
  switch (type) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_NONE;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_PASSWORD;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_EMAIL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_NUMBER;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_TEL;
    case ui::TEXT_INPUT_TYPE_URL:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_URL;
    case ui::TEXT_INPUT_TYPE_DATE:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_DATE;
    case ui::TEXT_INPUT_TYPE_TEXT:
    case ui::TEXT_INPUT_TYPE_SEARCH:
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TEXT_INPUT_TYPE_MONTH:
    case ui::TEXT_INPUT_TYPE_TIME:
    case ui::TEXT_INPUT_TYPE_WEEK:
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_TEXT;
  }
  NOTREACHED();
  return virtual_keyboard_private::ON_TEXT_INPUT_BOX_FOCUSED_TYPE_NONE;
}

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

    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_BOUNDS_CHANGED,
        virtual_keyboard_private::OnBoundsChanged::kEventName,
        std::move(event_args)));
    event->restrict_to_browser_context = context_;
    router->BroadcastEvent(std::move(event));
  }

  void OnKeyboardClosed() override {
    extensions::EventRouter* router = extensions::EventRouter::Get(context_);

    if (!router->HasEventListener(
            virtual_keyboard_private::OnKeyboardClosed::kEventName)) {
      return;
    }

    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CLOSED,
        virtual_keyboard_private::OnKeyboardClosed::kEventName,
        base::WrapUnique(new base::ListValue())));
    event->restrict_to_browser_context = context_;
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
  aura::Window* root_window = ash::Shell::GetTargetRootWindow();
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
      ash::GetRootWindowController(root_window);
  // Shell ime window container contains virtual keyboard windows and IME
  // windows(IME windows are created by chrome.app.window.create api). They
  // should never be overscrolled.
  return !root_window_controller
              ->GetContainer(ash::kShellWindowId_ImeWindowParentContainer)
              ->Contains(window);
}

void ChromeKeyboardUI::SetUpdateInputType(ui::TextInputType type) {
  // TODO(bshe): Need to check the affected window's profile once multi-profile
  // is supported.
  extensions::EventRouter* router =
      extensions::EventRouter::Get(browser_context());

  if (!router->HasEventListener(
          virtual_keyboard_private::OnTextInputBoxFocused::kEventName)) {
    return;
  }

  std::unique_ptr<base::ListValue> event_args(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> input_context(
      new base::DictionaryValue());
  input_context->SetString("type",
                           virtual_keyboard_private::ToString(
                               TextInputTypeToGeneratedInputTypeEnum(type)));
  event_args->Append(std::move(input_context));

  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_TEXT_INPUT_BOX_FOCUSED,
      virtual_keyboard_private::OnTextInputBoxFocused::kEventName,
      std::move(event_args)));
  event->restrict_to_browser_context = browser_context();
  router->DispatchEventToExtension(kVirtualKeyboardExtensionID,
                                   std::move(event));
}
