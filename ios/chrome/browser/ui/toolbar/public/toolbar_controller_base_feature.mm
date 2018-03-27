// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"

#include "base/command_line.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kMemexTabSwitcher{"MemexTabSwitcher",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const char kToolbarButtonPositionsSwitch[] = "toolbar-buttons-positions-switch";

ToolbarButtonPosition PositionForCurrentProcess() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kToolbarButtonPositionsSwitch)) {
    if (command_line->GetSwitchValueASCII(kToolbarButtonPositionsSwitch) ==
        "1") {
      return ToolbarButtonPositionNavigationBottomShareTop;
    } else if (command_line->GetSwitchValueASCII(
                   kToolbarButtonPositionsSwitch) == "2") {
      return ToolbarButtonPositionNavigationTop;
    } else {
      return ToolbarButtonPositionNavigationBottomNoTop;
    }
  }
  return ToolbarButtonPositionNavigationBottomNoTop;
}

const char kIconSearchButtonSwitch[] = "icon-search-button-switch";

extern const char kIconSearchButtonGrey[] = "icon-search-button-grey";
extern const char kIconSearchButtonColorful[] = "icon-search-button-colorful";
extern const char kIconSearchButtonMagnifying[] =
    "icon-search-button-magnifying";

ToolbarSearchButtonIcon IconForSearchButton() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kIconSearchButtonSwitch)) {
    if (command_line->GetSwitchValueASCII(kIconSearchButtonSwitch) ==
        kIconSearchButtonColorful) {
      return ToolbarSearchButtonIconColorful;
    } else if (command_line->GetSwitchValueASCII(kIconSearchButtonSwitch) ==
               kIconSearchButtonMagnifying) {
      return ToolbarSearchButtonIconMagnifying;
    } else {
      return ToolbarSearchButtonIconGrey;
    }
  }
  return ToolbarSearchButtonIconGrey;
}
