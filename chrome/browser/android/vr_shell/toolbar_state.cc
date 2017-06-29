// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/toolbar_state.h"

namespace vr_shell {

ToolbarState::ToolbarState()
    : gurl(GURL()),
      security_level(security_state::SecurityLevel::NONE),
      vector_icon(nullptr),
      should_display_url(true) {}

ToolbarState::ToolbarState(const GURL& url,
                           security_state::SecurityLevel level,
                           const gfx::VectorIcon* icon,
                           base::string16 verbose_text,
                           bool display_url)
    : gurl(url),
      security_level(level),
      vector_icon(icon),
      secure_verbose_text(verbose_text),
      should_display_url(display_url) {}

bool ToolbarState::operator==(const ToolbarState& other) const {
  return (gurl == other.gurl && security_level == other.security_level &&
          vector_icon == other.vector_icon &&
          should_display_url == other.should_display_url &&
          secure_verbose_text == other.secure_verbose_text);
}

bool ToolbarState::operator!=(const ToolbarState& other) const {
  return !(*this == other);
}

}  // namespace vr_shell
