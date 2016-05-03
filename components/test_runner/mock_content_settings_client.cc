// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/mock_content_settings_client.h"

#include "components/test_runner/layout_test_runtime_flags.h"
#include "components/test_runner/test_common.h"
#include "components/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace test_runner {

MockContentSettingsClient::MockContentSettingsClient(
    LayoutTestRuntimeFlags* layout_test_runtime_flags)
    : delegate_(nullptr), flags_(layout_test_runtime_flags) {}

MockContentSettingsClient::~MockContentSettingsClient() {}

bool MockContentSettingsClient::allowImage(bool enabled_per_settings,
                                           const blink::WebURL& image_url) {
  bool allowed = enabled_per_settings && flags_->images_allowed();
  if (flags_->dump_web_content_settings_client_callbacks() && delegate_) {
    delegate_->PrintMessage(
        std::string("MockContentSettingsClient: allowImage(") +
        NormalizeLayoutTestURL(image_url.string().utf8()) +
        "): " + (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool MockContentSettingsClient::allowMedia(const blink::WebURL& image_url) {
  bool allowed = flags_->media_allowed();
  if (flags_->dump_web_content_settings_client_callbacks() && delegate_)
    delegate_->PrintMessage(
        std::string("MockContentSettingsClient: allowMedia(") +
        NormalizeLayoutTestURL(image_url.string().utf8()) +
        "): " + (allowed ? "true" : "false") + "\n");
  return allowed;
}

bool MockContentSettingsClient::allowScript(bool enabled_per_settings) {
  return enabled_per_settings && flags_->scripts_allowed();
}

bool MockContentSettingsClient::allowScriptFromSource(
    bool enabled_per_settings,
    const blink::WebURL& script_url) {
  bool allowed = enabled_per_settings && flags_->scripts_allowed();
  if (flags_->dump_web_content_settings_client_callbacks() && delegate_) {
    delegate_->PrintMessage(
        std::string("MockContentSettingsClient: allowScriptFromSource(") +
        NormalizeLayoutTestURL(script_url.string().utf8()) + "): " +
        (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool MockContentSettingsClient::allowStorage(bool enabled_per_settings) {
  return flags_->storage_allowed();
}

bool MockContentSettingsClient::allowPlugins(bool enabled_per_settings) {
  return enabled_per_settings && flags_->plugins_allowed();
}

bool MockContentSettingsClient::allowDisplayingInsecureContent(
    bool enabled_per_settings,
    const blink::WebURL& url) {
  return enabled_per_settings || flags_->displaying_insecure_content_allowed();
}

bool MockContentSettingsClient::allowRunningInsecureContent(
    bool enabled_per_settings,
    const blink::WebSecurityOrigin& context,
    const blink::WebURL& url) {
  return enabled_per_settings || flags_->running_insecure_content_allowed();
}

bool MockContentSettingsClient::allowAutoplay(bool default_value) {
  return default_value || flags_->autoplay_allowed();
}

void MockContentSettingsClient::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
}

}  // namespace test_runner
