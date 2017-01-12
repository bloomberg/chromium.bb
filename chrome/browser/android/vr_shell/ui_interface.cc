// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_interface.h"

#include "chrome/browser/ui/webui/vr_shell/vr_shell_ui_message_handler.h"
#include "url/gurl.h"

namespace vr_shell {

UiInterface::UiInterface(Mode initial_mode, bool fullscreen) {
  SetMode(initial_mode);
  SetFullscreen(fullscreen);
}

UiInterface::~UiInterface() {}

void UiInterface::SetMode(Mode mode) {
  mode_ = mode;
  FlushModeState();
}

void UiInterface::SetMenuMode(bool enabled) {
  menu_mode_ = enabled;
  FlushModeState();
}

void UiInterface::SetFullscreen(bool enabled) {
  fullscreen_ = enabled;
  FlushModeState();
}

void UiInterface::FlushModeState() {
  updates_.SetInteger("mode", static_cast<int>(mode_));
  updates_.SetBoolean("menuMode", menu_mode_);
  updates_.SetBoolean("fullscreen", fullscreen_);
  FlushUpdates();
}

void UiInterface::SetSecurityLevel(int level) {
  updates_.SetInteger("securityLevel", level);
  FlushUpdates();
}

void UiInterface::SetWebVRSecureOrigin(bool secure) {
  updates_.SetBoolean("webVRSecureOrigin", secure);
  FlushUpdates();
}

void UiInterface::SetLoading(bool loading) {
  updates_.SetBoolean("loading", loading);
  FlushUpdates();
}

void UiInterface::SetLoadProgress(double progress) {
  updates_.SetDouble("loadProgress", progress);
  FlushUpdates();
}

void UiInterface::SetURL(const GURL& url) {
  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue);
  details->SetString("host", url.host());
  details->SetString("path", url.path());

  updates_.Set("url", std::move(details));
  FlushUpdates();
}

void UiInterface::OnDomContentsLoaded() {
  loaded_ = true;
#if defined(ENABLE_VR_SHELL_UI_DEV)
  updates_.SetBoolean("enableReloadUi", true);
#endif
  FlushUpdates();
}

void UiInterface::SetUiCommandHandler(UiCommandHandler* handler) {
  handler_ = handler;
}

void UiInterface::FlushUpdates() {
  if (loaded_ && handler_) {
    handler_->SendCommandToUi(updates_);
    updates_.Clear();
  }
}

}  // namespace vr_shell
