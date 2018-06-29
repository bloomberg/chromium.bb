// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

constexpr char kAssistantSettingsSpec[] = "googleassistant://settings";

// TODO(dmblack): Wire up actual Assistant Settings URL.
constexpr char kAssistantSettingsWebUiUrl[] = R"(data:text/html,
  <html>
    <body style="padding:32px;">
      <h3>Assistant Settings</h3>
      <p>Coming Soon! :)</p>
    </body>
  </html>
)";

}  // namespace

bool IsDeepLinkUrl(const GURL& url) {
  return IsAssistantSettingsDeepLink(url);
}

GURL CreateAssistantSettingsDeepLink() {
  return GURL(kAssistantSettingsSpec);
}

bool IsAssistantSettingsDeepLink(const GURL& url) {
  return url.spec() == kAssistantSettingsSpec;
}

base::Optional<GURL> GetWebUrl(const GURL& deep_link) {
  if (!IsWebDeepLink(deep_link))
    return base::nullopt;

  if (IsAssistantSettingsDeepLink(deep_link))
    return GURL(kAssistantSettingsWebUiUrl);

  NOTIMPLEMENTED();
  return base::nullopt;
}

bool IsWebDeepLink(const GURL& url) {
  return IsAssistantSettingsDeepLink(url);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
