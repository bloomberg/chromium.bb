// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_content_settings_client.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

bool AllowMixedContent(const blink::WebURL& url) {
  // We treat non-standard schemes as "secure" in the WebView to allow them to
  // be used for request interception.
  // TODO(benm): Tighten this restriction by requiring embedders to register
  // their custom schemes? See b/9420953.
  GURL gurl(url);
  return !gurl.IsStandard();
}

}

AwContentSettingsClient::AwContentSettingsClient(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->setContentSettingsClient(this);
}

AwContentSettingsClient::~AwContentSettingsClient() {
}

bool AwContentSettingsClient::allowRunningInsecureContent(
      bool enabled_per_settings,
      const blink::WebSecurityOrigin& origin,
      const blink::WebURL& url) {
  return enabled_per_settings ? true : AllowMixedContent(url);
}

void AwContentSettingsClient::OnDestruct() {
  delete this;
}

}  // namespace android_webview
