// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

#include "base/logging.h"

ImmersiveModeControllerStub::ImmersiveModeControllerStub() {
}

ImmersiveModeControllerStub::~ImmersiveModeControllerStub() {
}

void ImmersiveModeControllerStub::Init(BrowserView* browser_view) {
}

void ImmersiveModeControllerStub::SetEnabled(bool enabled) {
  NOTREACHED();
}

bool ImmersiveModeControllerStub::IsEnabled() const {
  return false;
}

bool ImmersiveModeControllerStub::ShouldHideTabIndicators() const {
  return false;
}

bool ImmersiveModeControllerStub::ShouldHideTopViews() const {
  return false;
}

bool ImmersiveModeControllerStub::IsRevealed() const {
  return false;
}

void ImmersiveModeControllerStub::MaybeStackViewAtTop() {
}

void ImmersiveModeControllerStub::MaybeStartReveal() {
}

void ImmersiveModeControllerStub::CancelReveal() {
}

ImmersiveModeController::RevealedLock*
    ImmersiveModeControllerStub::GetRevealedLock() {
  return NULL;
}
