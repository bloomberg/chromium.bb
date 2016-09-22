// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/vr_shell/vr_shell_ui_message_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "content/public/browser/web_ui.h"

VrShellUIMessageHandler::VrShellUIMessageHandler() {}

VrShellUIMessageHandler::~VrShellUIMessageHandler() {}

void VrShellUIMessageHandler::RegisterMessages() {
  vr_shell_ = vr_shell::VrShell::GetWeakPtr();

  web_ui()->RegisterMessageCallback(
      "domLoaded", base::Bind(&VrShellUIMessageHandler::HandleDomLoaded,
                              base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addMesh", base::Bind(&VrShellUIMessageHandler::HandleAddMesh,
                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeMesh", base::Bind(&VrShellUIMessageHandler::HandleRemoveMesh,
                               base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "doAction", base::Bind(&VrShellUIMessageHandler::HandleDoAction,
                             base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addAnimations", base::Bind(&VrShellUIMessageHandler::HandleAddAnimations,
                                  base::Unretained(this)));
}

void VrShellUIMessageHandler::HandleDomLoaded(const base::ListValue* args) {
  if (!vr_shell_)
    return;

  vr_shell_->OnDomContentsLoaded();

  // TODO(bshe): Get size from native side directly.
  CHECK_EQ(2u, args->GetSize());
  int width, height;
  CHECK(args->GetInteger(0, &width));
  CHECK(args->GetInteger(1, &height));
  vr_shell_->SetUiTextureSize(width, height);
}

void VrShellUIMessageHandler::HandleAddMesh(const base::ListValue* args) {
  NOTIMPLEMENTED();
}

void VrShellUIMessageHandler::HandleRemoveMesh(const base::ListValue* args) {
  NOTIMPLEMENTED();
}

void VrShellUIMessageHandler::HandleAddAnimations(const base::ListValue* args) {
  NOTIMPLEMENTED();
}

void VrShellUIMessageHandler::HandleDoAction(const base::ListValue* args) {
  NOTIMPLEMENTED();
}
