// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/pointer_handler2.h"

#include "base/basictypes.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace chromeos {
namespace options2 {

PointerHandler::PointerHandler() {
}

PointerHandler::~PointerHandler() {
}

void PointerHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "pointerOverlayTitle", IDS_OPTIONS_POINTER_OVERLAY_TITLE },
    { "pointerOverlaySectionTitleTouchpad",
      IDS_OPTIONS_POINTER_OVERLAY_SECTION_TITLE_TOUCHPAD },
    { "pointerOverlaySectionTitleMouse",
      IDS_OPTIONS_POINTER_OVERLAY_SECTION_TITLE_MOUSE },
    { "enableTapToClick",
      IDS_OPTIONS_SETTINGS_TAP_TO_CLICK_ENABLED_DESCRIPTION },
    { "naturalScroll",
      IDS_OPTIONS_SETTINGS_NATURAL_SCROLL_DESCRIPTION },
    { "primaryMouseRight",
      IDS_OPTIONS_SETTINGS_PRIMARY_MOUSE_RIGHT_DESCRIPTION },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
}


void PointerHandler::TouchpadExists(bool exists) {
  base::FundamentalValue val(exists);
  web_ui()->CallJavascriptFunction("PointerOverlay.showTouchpadControls", val);
}

void PointerHandler::MouseExists(bool exists) {
  base::FundamentalValue val(exists);
  web_ui()->CallJavascriptFunction("PointerOverlay.showMouseControls", val);
}

}  // namespace options2
}  // namespace chromeos
