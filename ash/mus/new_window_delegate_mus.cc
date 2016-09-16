// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/new_window_delegate_mus.h"

#include "base/logging.h"
#include "services/shell/public/cpp/connector.h"

namespace ash {
namespace mus {

NewWindowDelegateMus::NewWindowDelegateMus() {}

NewWindowDelegateMus::~NewWindowDelegateMus() {}

void NewWindowDelegateMus::NewTab() {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::NewWindow(bool incognito) {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::OpenFileManager() {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::OpenCrosh() {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::OpenGetHelp() {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::RestoreTab() {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::ShowKeyboardOverlay() {
  // TODO: http://crbug.com/647433.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::ShowTaskManager() {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

void NewWindowDelegateMus::OpenFeedbackPage() {
  // TODO: http://crbug.com/631836.
  NOTIMPLEMENTED();
}

}  // namespace mus
}  // namespace ash
