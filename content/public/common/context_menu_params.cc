// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/context_menu_params.h"

namespace content {

const int32 CustomContextMenuContext::kCurrentRenderWidget = kint32max;

CustomContextMenuContext::CustomContextMenuContext()
    : is_pepper_menu(false),
      request_id(0),
      render_widget_id(kCurrentRenderWidget) {
}

ContextMenuParams::ContextMenuParams()
    : media_type(WebKit::WebContextMenuData::MediaTypeNone),
      x(0),
      y(0),
      is_image_blocked(false),
      frame_id(0),
      media_flags(0),
      misspelling_hash(0),
      speech_input_enabled(false),
      spellcheck_enabled(false),
      is_editable(false),
#if defined(OS_MACOSX)
      writing_direction_default(
          WebKit::WebContextMenuData::CheckableMenuItemDisabled),
      writing_direction_left_to_right(
          WebKit::WebContextMenuData::CheckableMenuItemEnabled),
      writing_direction_right_to_left(
          WebKit::WebContextMenuData::CheckableMenuItemEnabled),
#endif  // OS_MACOSX
      edit_flags(0),
      referrer_policy(WebKit::WebReferrerPolicyDefault) {
}

ContextMenuParams::~ContextMenuParams() {
}

}  // namespace content
