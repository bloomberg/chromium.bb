// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_interface.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/vr_omnibox.h"
#include "chrome/browser/ui/webui/vr_shell/vr_shell_ui_message_handler.h"
#include "url/gurl.h"

namespace vr_shell {

UiInterface::UiInterface(Mode initial_mode)
    : omnibox_(base::MakeUnique<VrOmnibox>(this)) {
  SetMode(initial_mode);
}

UiInterface::~UiInterface() {}

void UiInterface::SetMode(Mode mode) {
  mode_ = mode;
  FlushModeState();
}

void UiInterface::SetFullscreen(bool enabled) {
  fullscreen_ = enabled;
  FlushModeState();
}

void UiInterface::HandleOmniboxInput(const base::DictionaryValue& input) {
  omnibox_->HandleInput(input);
}

void UiInterface::SetOmniboxSuggestions(
    std::unique_ptr<base::Value> suggestions) {
  updates_.Set("suggestions", std::move(suggestions));
  FlushUpdates();
}

void UiInterface::FlushModeState() {
  updates_.SetInteger("mode", static_cast<int>(mode_));
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

void UiInterface::InitTabList() {
  tab_list_ = base::MakeUnique<base::ListValue>();
}

void UiInterface::AppendToTabList(bool incognito,
                                  int id,
                                  const base::string16& title) {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetBoolean("incognito", incognito);
  dict->SetInteger("id", id);
  dict->SetString("title", title);
  tab_list_->Append(std::move(dict));
}

void UiInterface::FlushTabList() {
  updates_.Set("setTabs", std::move(tab_list_));
  FlushUpdates();
}

void UiInterface::UpdateTab(bool incognito, int id, const std::string& title) {
  auto details = base::MakeUnique<base::DictionaryValue>();
  details->SetBoolean("incognito", incognito);
  details->SetInteger("id", id);
  details->SetString("title", title);
  updates_.Set("updateTab", std::move(details));
}

void UiInterface::RemoveTab(bool incognito, int id) {
  auto details = base::MakeUnique<base::DictionaryValue>();
  details->SetBoolean("incognito", incognito);
  details->SetInteger("id", id);
  updates_.Set("removeTab", std::move(details));
}

void UiInterface::SetURL(const GURL& url) {
  auto details = base::MakeUnique<base::DictionaryValue>();
  details->SetString("host", url.host());
  details->SetString("path", url.path());

  updates_.Set("url", std::move(details));
  FlushUpdates();
}

void UiInterface::HandleAppButtonClicked() {
  updates_.SetBoolean("appButtonClicked", true);
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
