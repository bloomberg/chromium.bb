// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/new_window_controller.h"

#include <utility>

namespace ash {

NewWindowController::NewWindowController() = default;

NewWindowController::~NewWindowController() = default;

void NewWindowController::BindRequest(
    mojom::NewWindowControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void NewWindowController::SetClient(
    mojom::NewWindowClientAssociatedPtrInfo client) {
  client_.Bind(std::move(client));
}

// TODO(crbug.com/755448): Remove this when the new shortcut viewer is enabled.
void NewWindowController::ShowKeyboardOverlay() {
  // TODO(estade): implement this here rather than passing off to |client_|.
  if (client_)
    client_->ShowKeyboardOverlay();
}

void NewWindowController::NewTab() {
  if (client_)
    client_->NewTab();
}

void NewWindowController::NewWindow(bool incognito) {
  if (client_)
    client_->NewWindow(incognito);
}

void NewWindowController::OpenFileManager() {
  if (client_)
    client_->OpenFileManager();
}

void NewWindowController::OpenCrosh() {
  if (client_)
    client_->OpenCrosh();
}

void NewWindowController::OpenGetHelp() {
  if (client_)
    client_->OpenGetHelp();
}

void NewWindowController::RestoreTab() {
  if (client_)
    client_->RestoreTab();
}

void NewWindowController::ShowKeyboardShortcutViewer() {
  if (client_)
    client_->ShowKeyboardShortcutViewer();
}

void NewWindowController::ShowTaskManager() {
  if (client_)
    client_->ShowTaskManager();
}

void NewWindowController::OpenFeedbackPage() {
  if (client_)
    client_->OpenFeedbackPage();
}

}  // namespace ash
