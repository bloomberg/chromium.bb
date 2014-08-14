// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/help_overlay_handler.h"

#include "base/values.h"
#include "chrome/browser/ui/webui/help/help_handler.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace options {

HelpOverlayHandler::HelpOverlayHandler() {
}

HelpOverlayHandler::~HelpOverlayHandler() {
}

void HelpOverlayHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  RegisterTitle(localized_strings, "aboutOverlay", IDS_ABOUT_TITLE);
  HelpHandler::GetLocalizedValues(localized_strings);
}

void HelpOverlayHandler::RegisterMessages() {
  if (::switches::AboutInSettingsEnabled())
    web_ui()->AddMessageHandler(new HelpHandler());
}

}  // namespace options
