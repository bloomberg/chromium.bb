// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/open_from_clipboard/clipboard_recent_content_features.h"
#include "components/variations/variations_associated_data.h"
#include "url/url_constants.h"

namespace {
ClipboardRecentContent* g_clipboard_recent_content = nullptr;

}  // namespace

ClipboardRecentContent::ClipboardRecentContent() {}

ClipboardRecentContent::~ClipboardRecentContent() {
}

// static
ClipboardRecentContent* ClipboardRecentContent::GetInstance() {
  return g_clipboard_recent_content;
}

// static
void ClipboardRecentContent::SetInstance(
    std::unique_ptr<ClipboardRecentContent> new_instance) {
  delete g_clipboard_recent_content;
  g_clipboard_recent_content = new_instance.release();
}

// static
base::TimeDelta ClipboardRecentContent::MaximumAgeOfClipboard() {
  // Identify the current setting for this parameter from the feature, using
  // 3600 seconds (1 hour) as a default if the parameter is not set.
  int value = variations::GetVariationParamByFeatureAsInt(
      kClipboardMaximumAge, kClipboardMaximumAgeParam, 3600);
  return base::TimeDelta::FromSeconds(value);
}
