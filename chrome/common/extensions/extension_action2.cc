// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_action2.h"

#include "base/logging.h"

const int ExtensionAction2::kDefaultTabId = -1;

void ExtensionAction2::SetDefaultIcon(const std::string& path) {
  default_icon_path_ = path;
  icon_.erase(kDefaultTabId);
}

void ExtensionAction2::SetDefaultIcon(int icon_index) {
  if (static_cast<size_t>(icon_index) >= icon_paths_.size()) {
    NOTREACHED();
    return;
  }

  SetDefaultIcon(icon_paths_[icon_index]);
}

void ExtensionAction2::ClearAllValuesForTab(int tab_id) {
  title_.erase(tab_id);
  icon_.erase(tab_id);
  badge_text_.erase(tab_id);
  badge_background_color_.erase(tab_id);
  badge_text_color_.erase(tab_id);
}
