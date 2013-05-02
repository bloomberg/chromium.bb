// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_keyboard_controller_proxy.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ipc/ipc_message_macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_controller.h"

AshKeyboardControllerProxy::AshKeyboardControllerProxy() {}

AshKeyboardControllerProxy::~AshKeyboardControllerProxy() {}

void AshKeyboardControllerProxy::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(
      params, web_contents()->GetRenderViewHost());
}

content::BrowserContext* AshKeyboardControllerProxy::GetBrowserContext() {
  return ProfileManager::GetDefaultProfile();
}

ui::InputMethod* AshKeyboardControllerProxy::GetInputMethod() {
  aura::Window* root_window = ash::Shell::GetInstance()->GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
}

void AshKeyboardControllerProxy::SetupWebContents(
    content::WebContents* contents) {
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(ProfileManager::GetDefaultProfile(),
                                      this));
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

void AshKeyboardControllerProxy::OnKeyboardBoundsChanged(
    const gfx::Rect& keyboard_bounds) {
  if (keyboard_bounds.IsEmpty())
    return;

  gfx::Rect showing_area =
      ash::DisplayController::GetPrimaryDisplay().work_area();
  GetInputMethod()->GetTextInputClient()->EnsureCaretInRect(showing_area);
}
