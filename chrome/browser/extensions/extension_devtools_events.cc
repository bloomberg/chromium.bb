// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_devtools_events.h"

#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

// These string constants and the formats used in this file must stay
// in sync with chrome/renderer/resources/extension_process_bindings.js
static const char kDevToolsEventPrefix[] = "devtools.";
static const char kOnPageEventName[] = "onPageEvent";
static const char kOnTabCloseEventName[] = "onTabClose";

// static
bool ExtensionDevToolsEvents::IsDevToolsEventName(
    const std::string& event_name, int* tab_id) {
  // We only care about events of the form "devtools.34.*", where 34 is
  // a tab id.
  if (IsStringASCII(event_name) &&
      StartsWithASCII(event_name,
                      kDevToolsEventPrefix,
                      true /* case_sensitive */)) {
    // At this point we want something like "4.onPageEvent"
    std::vector<std::string> parts;
    base::SplitString(
        event_name.substr(strlen(kDevToolsEventPrefix)), '.', &parts);
    if (parts.size() == 2 && base::StringToInt(parts[0], tab_id)) {
      return true;
    }
  }
  return false;
}

// static
std::string ExtensionDevToolsEvents::OnPageEventNameForTab(int tab_id) {
  return base::StringPrintf("%s%d.%s",
                            kDevToolsEventPrefix,
                            tab_id,
                            kOnPageEventName);
}

// static
std::string ExtensionDevToolsEvents::OnTabCloseEventNameForTab(int tab_id) {
  return base::StringPrintf("%s%d.%s",
                            kDevToolsEventPrefix,
                            tab_id,
                            kOnTabCloseEventName);
}

