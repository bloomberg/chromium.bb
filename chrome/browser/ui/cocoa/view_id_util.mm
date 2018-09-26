// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/view_id_util.h"

#import <Cocoa/Cocoa.h>

#include <map>
#include <utility>

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"

namespace {

// TODO(suzhe): After migrating to Mac OS X 10.6, we may use Objective-C's new
// "Associative References" feature to attach the ViewID to the view directly
// rather than using a separated map.
typedef std::map<NSView*, ViewID> ViewIDMap;

ViewIDMap* GetViewIDMap() {
  static auto* view_id_map = new ViewIDMap();
  return view_id_map;
}

// Returns the view's nearest descendant (including itself) with a specific
// ViewID, or nil if no subview has that ViewID.
NSView* FindViewWithID(NSView* view, ViewID viewID) {
  if ([view viewID] == viewID)
    return view;

  for (NSView* subview in [view subviews]) {
    NSView* result = FindViewWithID(subview, viewID);
    if (result != nil)
      return result;
  }
  return nil;
}

}  // anonymous namespace

namespace view_id_util {

void SetID(NSView* view, ViewID viewID) {
  DCHECK(view);
  DCHECK(viewID != VIEW_ID_NONE);
  // We handle VIEW_ID_TAB_0 to VIEW_ID_TAB_LAST in GetView() function directly.
  DCHECK(!((viewID >= VIEW_ID_TAB_0) && (viewID <= VIEW_ID_TAB_LAST)));
  (*GetViewIDMap())[view] = viewID;
}

void UnsetID(NSView* view) {
  DCHECK(view);
  GetViewIDMap()->erase(view);
}

NSView* GetView(NSWindow* window, ViewID viewID) {
  DCHECK(viewID != VIEW_ID_NONE);
  DCHECK(window);

  return FindViewWithID([[window contentView] superview], viewID);
}

}  // namespace view_id_util

@implementation NSView (ViewID)

- (ViewID)viewID {
  const ViewIDMap* map = GetViewIDMap();
  ViewIDMap::const_iterator iter = map->find(self);
  return iter != map->end() ? iter->second : VIEW_ID_NONE;
}

- (NSView*)ancestorWithViewID:(ViewID)viewID {
  NSView* ancestor = self;
  while (ancestor && [ancestor viewID] != viewID)
    ancestor = [ancestor superview];
  return ancestor;
}

@end
