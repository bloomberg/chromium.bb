// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_

// Put utility functions only used by //chrome code here. If a function declared
// here would be meaningfully shared with other platforms, consider moving it to
// components/content_settings/core/browser/content_settings_utils.h.

namespace content_settings {

// UMA statistics for the mixed content shield
enum MixedScriptAction {
  MIXED_SCRIPT_ACTION_DISPLAYED_SHIELD = 0,
  MIXED_SCRIPT_ACTION_DISPLAYED_BUBBLE,
  MIXED_SCRIPT_ACTION_CLICKED_ALLOW,
  MIXED_SCRIPT_ACTION_CLICKED_LEARN_MORE,
  MIXED_SCRIPT_ACTION_COUNT
};

void RecordMixedScriptAction(MixedScriptAction action);

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_UTILS_H_
