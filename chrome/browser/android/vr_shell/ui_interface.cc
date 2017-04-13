// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_interface.h"

#include "url/gurl.h"

namespace vr_shell {

UiInterface::UiInterface(Mode initial_mode) : mode_(initial_mode) {}

void UiInterface::SetMode(Mode mode) {
  mode_ = mode;
}

void UiInterface::SetFullscreen(bool enabled) {
  fullscreen_ = enabled;
}

void UiInterface::SetSecurityLevel(int level) {}

void UiInterface::SetWebVRSecureOrigin(bool secure) {}

void UiInterface::SetLoading(bool loading) {}

void UiInterface::SetLoadProgress(double progress) {}

void UiInterface::InitTabList() {}

void UiInterface::AppendToTabList(bool incognito,
                                  int id,
                                  const base::string16& title) {}

void UiInterface::UpdateTab(bool incognito, int id, const std::string& title) {}

void UiInterface::FlushTabList() {}

void UiInterface::RemoveTab(bool incognito, int id) {}

void UiInterface::SetURL(const GURL& url) {}

void UiInterface::HandleAppButtonGesturePerformed(Direction direction) {}

void UiInterface::HandleAppButtonClicked() {}

void UiInterface::SetHistoryButtonsEnabled(bool can_go_back,
                                           bool can_go_forward) {}

}  // namespace vr_shell
