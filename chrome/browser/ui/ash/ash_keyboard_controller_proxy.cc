// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_keyboard_controller_proxy.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/virtual_keyboard_private.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message_macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/keyboard/keyboard_controller.h"

namespace virtual_keyboard_private = extensions::api::virtual_keyboard_private;
typedef virtual_keyboard_private::OnTextInputBoxFocused::Context Context;

namespace {

const char* kVirtualKeyboardExtensionID = "mppnpdlheglhdfmldimlhpnegondlapf";

Context::Type TextInputTypeToGeneratedInputTypeEnum(ui::TextInputType type) {
  switch (type) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return Context::TYPE_NONE;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return Context::TYPE_PASSWORD;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return Context::TYPE_EMAIL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return Context::TYPE_NUMBER;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return Context::TYPE_TEL;
    case ui::TEXT_INPUT_TYPE_URL:
      return Context::TYPE_URL;
    case ui::TEXT_INPUT_TYPE_DATE:
      return Context::TYPE_DATE;
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
      return Context::TYPE_TEXT;
  }
  NOTREACHED();
  return Context::TYPE_NONE;
}

}  // namespace

AshKeyboardControllerProxy::AshKeyboardControllerProxy() {}

AshKeyboardControllerProxy::~AshKeyboardControllerProxy() {}

void AshKeyboardControllerProxy::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(
      params, web_contents()->GetRenderViewHost());
}

content::BrowserContext* AshKeyboardControllerProxy::GetBrowserContext() {
  return ProfileManager::GetActiveUserProfile();
}

ui::InputMethod* AshKeyboardControllerProxy::GetInputMethod() {
  aura::Window* root_window = ash::Shell::GetInstance()->GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
}

void AshKeyboardControllerProxy::RequestAudioInput(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) {
  const extensions::Extension* extension = NULL;
  GURL origin(request.security_origin);
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionService* extensions_service =
        extensions::ExtensionSystem::Get(
            GetBrowserContext())->extension_service();
    extension = extensions_service->extensions()->GetByID(origin.host());
    DCHECK(extension);
  }

  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

void AshKeyboardControllerProxy::SetupWebContents(
    content::WebContents* contents) {
  extension_function_dispatcher_.reset(
      new extensions::ExtensionFunctionDispatcher(GetBrowserContext(), this));
  extensions::SetViewType(contents, extensions::VIEW_TYPE_VIRTUAL_KEYBOARD);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
  Observe(contents);
}

extensions::WindowController*
    AshKeyboardControllerProxy::GetExtensionWindowController() const {
  // The keyboard doesn't have a window controller.
  return NULL;
}

content::WebContents*
    AshKeyboardControllerProxy::GetAssociatedWebContents() const {
  return web_contents();
}

bool AshKeyboardControllerProxy::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AshKeyboardControllerProxy, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AshKeyboardControllerProxy::ShowKeyboardContainer(
    aura::Window* container) {
  // TODO(bshe): Implement logic to decide which root window should display
  // virtual keyboard. http://crbug.com/303429
  if (container->GetRootWindow() != ash::Shell::GetPrimaryRootWindow())
    NOTIMPLEMENTED();

  KeyboardControllerProxy::ShowKeyboardContainer(container);
}

void AshKeyboardControllerProxy::SetUpdateInputType(ui::TextInputType type) {
  // TODO(bshe): Need to check the affected window's profile once multi-profile
  // is supported.
  content::BrowserContext* context = GetBrowserContext();
  extensions::EventRouter* router = extensions::EventRouter::Get(context);

  if (!router->HasEventListener(
          virtual_keyboard_private::OnTextInputBoxFocused::kEventName)) {
    return;
  }

  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  scoped_ptr<base::DictionaryValue> input_context(new base::DictionaryValue());
  input_context->SetString("type",
      Context::ToString(TextInputTypeToGeneratedInputTypeEnum(type)));
  event_args->Append(input_context.release());

  scoped_ptr<extensions::Event> event(new extensions::Event(
      virtual_keyboard_private::OnTextInputBoxFocused::kEventName,
      event_args.Pass()));
  event->restrict_to_browser_context = context;
  router->DispatchEventToExtension(kVirtualKeyboardExtensionID, event.Pass());
}
