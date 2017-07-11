// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/toolbar_state.h"

namespace vr {

ToolbarState::ToolbarState()
    : gurl(GURL()),
      security_level(security_state::SecurityLevel::NONE),
      vector_icon(nullptr),
      should_display_url(true),
      offline_page(false) {}

ToolbarState::ToolbarState(const GURL& url,
                           security_state::SecurityLevel level,
                           const gfx::VectorIcon* icon,
                           base::string16 verbose_text,
                           bool display_url,
                           bool offline)
    : gurl(url),
      security_level(level),
      vector_icon(icon),
      secure_verbose_text(verbose_text),
      should_display_url(display_url),
      offline_page(offline) {}

ToolbarState::ToolbarState(const ToolbarState& other) = default;

bool ToolbarState::operator==(const ToolbarState& other) const {
  return (gurl == other.gurl && security_level == other.security_level &&
          vector_icon == other.vector_icon &&
          should_display_url == other.should_display_url &&
          secure_verbose_text == other.secure_verbose_text &&
          offline_page == other.offline_page);
}

bool ToolbarState::operator!=(const ToolbarState& other) const {
  return !(*this == other);
}

}  // namespace vr
