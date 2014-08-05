// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/web_permissions.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/test_common.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

WebPermissions::WebPermissions() : delegate_(0) {
  Reset();
}

WebPermissions::~WebPermissions() {}

bool WebPermissions::allowImage(bool enabled_per_settings,
                                const blink::WebURL& image_url) {
  bool allowed = enabled_per_settings && images_allowed_;
  if (dump_callbacks_ && delegate_) {
    delegate_->printMessage(std::string("PERMISSION CLIENT: allowImage(") +
                            NormalizeLayoutTestURL(image_url.spec()) + "): " +
                            (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool WebPermissions::allowMedia(const blink::WebURL& image_url) {
  bool allowed = media_allowed_;
  if (dump_callbacks_ && delegate_)
    delegate_->printMessage(std::string("PERMISSION CLIENT: allowMedia(") +
                            NormalizeLayoutTestURL(image_url.spec()) + "): " +
                            (allowed ? "true" : "false") + "\n");
  return allowed;
}

bool WebPermissions::allowScriptFromSource(bool enabled_per_settings,
                                           const blink::WebURL& scriptURL) {
  bool allowed = enabled_per_settings && scripts_allowed_;
  if (dump_callbacks_ && delegate_) {
    delegate_->printMessage(
        std::string("PERMISSION CLIENT: allowScriptFromSource(") +
        NormalizeLayoutTestURL(scriptURL.spec()) + "): " +
        (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool WebPermissions::allowStorage(bool) {
  return storage_allowed_;
}

bool WebPermissions::allowPlugins(bool enabled_per_settings) {
  return enabled_per_settings && plugins_allowed_;
}

bool WebPermissions::allowDisplayingInsecureContent(
    bool enabled_per_settings,
    const blink::WebSecurityOrigin&,
    const blink::WebURL&) {
  return enabled_per_settings || displaying_insecure_content_allowed_;
}

bool WebPermissions::allowRunningInsecureContent(
    bool enabled_per_settings,
    const blink::WebSecurityOrigin&,
    const blink::WebURL&) {
  return enabled_per_settings || running_insecure_content_allowed_;
}

void WebPermissions::SetImagesAllowed(bool images_allowed) {
  images_allowed_ = images_allowed;
}

void WebPermissions::SetMediaAllowed(bool media_allowed) {
  media_allowed_ = media_allowed;
}

void WebPermissions::SetScriptsAllowed(bool scripts_allowed) {
  scripts_allowed_ = scripts_allowed;
}

void WebPermissions::SetStorageAllowed(bool storage_allowed) {
  storage_allowed_ = storage_allowed;
}

void WebPermissions::SetPluginsAllowed(bool plugins_allowed) {
  plugins_allowed_ = plugins_allowed;
}

void WebPermissions::SetDisplayingInsecureContentAllowed(bool allowed) {
  displaying_insecure_content_allowed_ = allowed;
}

void WebPermissions::SetRunningInsecureContentAllowed(bool allowed) {
  running_insecure_content_allowed_ = allowed;
}

void WebPermissions::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
}

void WebPermissions::SetDumpCallbacks(bool dump_callbacks) {
  dump_callbacks_ = dump_callbacks;
}

void WebPermissions::Reset() {
  dump_callbacks_ = false;
  images_allowed_ = true;
  media_allowed_ = true;
  scripts_allowed_ = true;
  storage_allowed_ = true;
  plugins_allowed_ = true;
  displaying_insecure_content_allowed_ = false;
  running_insecure_content_allowed_ = false;
}

}  // namespace content
