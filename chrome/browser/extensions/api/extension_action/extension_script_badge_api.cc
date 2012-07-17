// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_script_badge_api.h"

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "content/public/browser/web_contents.h"

ScriptBadgeGetAttentionFunction::~ScriptBadgeGetAttentionFunction() {}

bool ScriptBadgeGetAttentionFunction::RunExtensionAction() {
  tab_helper().location_bar_controller()->GetAttentionFor(extension_id());
  return true;
}
