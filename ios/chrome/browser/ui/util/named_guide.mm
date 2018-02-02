// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/named_guide.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Named guide constants.
GuideName* const kOmniboxGuide = @"kOmniboxGuide";
GuideName* const kBackButtonGuide = @"kBackButtonGuide";
GuideName* const kForwardButtonGuide = @"kForwardButtonGuide";
GuideName* const kTabSwitcherGuide = @"kTabSwitcherGuide";
GuideName* const kToolsMenuGuide = @"kToolsMenuGuide";

UILayoutGuide* FindNamedGuide(GuideName* name, UIView* view) {
  while (view) {
    for (UILayoutGuide* guide in view.layoutGuides) {
      if ([guide.identifier isEqualToString:name])
        return guide;
    }
    view = view.superview;
  }
  return nil;
}

UILayoutGuide* AddNamedGuide(GuideName* name, UIView* view) {
  DCHECK(!FindNamedGuide(name, view));
  UILayoutGuide* guide = [[UILayoutGuide alloc] init];
  guide.identifier = name;
  [view addLayoutGuide:guide];
  return guide;
}

void ConstrainNamedGuideToView(GuideName* guideName, UIView* view) {
  UILayoutGuide* layoutGuide = FindNamedGuide(guideName, view);
  DCHECK(layoutGuide);
  AddSameConstraints(view, layoutGuide);
}
