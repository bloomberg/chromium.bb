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

}  // namespace

bool IsDeepLinkUrl(const GURL& url) {
  return url.spec() == kAssistantSettingsSpec;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
