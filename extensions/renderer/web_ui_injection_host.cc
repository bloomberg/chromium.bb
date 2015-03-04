// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/web_ui_injection_host.h"

WebUIInjectionHost::WebUIInjectionHost(const HostID& host_id)
  : InjectionHost(host_id),
    url_(host_id.id()) {
}

WebUIInjectionHost::~WebUIInjectionHost() {
}


std::string WebUIInjectionHost::GetContentSecurityPolicy() const {
  return std::string();
}

const GURL& WebUIInjectionHost::url() const {
  return url_;
}

const std::string& WebUIInjectionHost::name() const {
  return id().id();
}

extensions::PermissionsData::AccessType WebUIInjectionHost::CanExecuteOnFrame(
      const GURL& document_url,
      const GURL& top_frame_url,
      int tab_id,
      bool is_declarative) const {
  // Content scripts are allowed to inject on webviews created by WebUI.
  return extensions::PermissionsData::AccessType::ACCESS_ALLOWED;
}

bool WebUIInjectionHost::ShouldNotifyBrowserOfInjection() const {
  // We don't notify browser of any injection made from WebUI, since the
  // decision for injection is made in the render.
  return false;
}
