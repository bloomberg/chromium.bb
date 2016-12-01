// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/vr_shell/vr_shell_ui_message_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "content/public/browser/web_ui.h"

VrShellUIMessageHandler::VrShellUIMessageHandler() = default;

VrShellUIMessageHandler::~VrShellUIMessageHandler() {
  if (vr_shell_) {
    vr_shell_->GetUiInterface()->SetUiCommandHandler(nullptr);
  }
}

void VrShellUIMessageHandler::RegisterMessages() {
  vr_shell_ = vr_shell::VrShell::GetWeakPtr(web_ui()->GetWebContents());

  web_ui()->RegisterMessageCallback(
      "domLoaded", base::Bind(&VrShellUIMessageHandler::HandleDomLoaded,
                              base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateScene", base::Bind(&VrShellUIMessageHandler::HandleUpdateScene,
                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "doAction", base::Bind(&VrShellUIMessageHandler::HandleDoAction,
                             base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setUiCssSize", base::Bind(&VrShellUIMessageHandler::HandleSetUiCssSize,
                                 base::Unretained(this)));
}

void VrShellUIMessageHandler::HandleDomLoaded(const base::ListValue* args) {
  AllowJavascript();
}

void VrShellUIMessageHandler::OnJavascriptAllowed() {
  // If we don't have a VR Shell here, it means either the user manually loaded
  // this webui page and we want to silently fail to connect to native vr shell,
  // or VR Shell was deleted, and this webui content is also about to be
  // deleted.
  if (!vr_shell_)
    return;
  vr_shell_->GetUiInterface()->SetUiCommandHandler(this);
  vr_shell_->OnDomContentsLoaded();
}

void VrShellUIMessageHandler::HandleUpdateScene(const base::ListValue* args) {
  if (!vr_shell_)
    return;

  // Copy the update instructions and handle them on the render thread.
  auto cb = base::Bind(&vr_shell::UiScene::HandleCommands,
                       base::Unretained(vr_shell_->GetScene()),
                       base::Owned(args->CreateDeepCopy().release()),
                       vr_shell::UiScene::TimeInMicroseconds());
  vr_shell_->QueueTask(cb);
}

void VrShellUIMessageHandler::HandleDoAction(const base::ListValue* args) {
  int action;
  CHECK(args->GetInteger(0, &action));
  if (vr_shell_) {
    vr_shell_->DoUiAction((vr_shell::UiAction) action);
  }
}

void VrShellUIMessageHandler::HandleSetUiCssSize(const base::ListValue* args) {
  CHECK(args->GetSize() == 3);
  double width, height, dpr;
  CHECK(args->GetDouble(0, &width));
  CHECK(args->GetDouble(1, &height));
  CHECK(args->GetDouble(2, &dpr));
  if (vr_shell_) {
    vr_shell_->SetUiCssSize(width, height, dpr);
  }
}

void VrShellUIMessageHandler::SendCommandToUi(const base::Value& value) {
  CallJavascriptFunction("vrShellUi.command", value);
}
