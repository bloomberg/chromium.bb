// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <execinfo.h>

#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/browser_accessibility.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"

using webkit_glue::WebAccessibility;

namespace {

// Returns an autoreleased copy of the WebAccessibility's attribute.
NSString* NSStringForWebAccessibilityAttribute(
    std::map<int32, string16>& attributes,
    WebAccessibility::Attribute attribute) {
  std::map<int32, string16>::iterator iter =
      attributes.find(attribute);
  NSString* returnValue = @"";
  if (iter != attributes.end()) {
    returnValue = base::SysUTF16ToNSString(iter->second);
  }
  return returnValue;
}

struct RoleEntry {
  WebAccessibility::Role value;
  NSString* string;
};

static const RoleEntry roles[] = {
  { WebAccessibility::ROLE_NONE, NSAccessibilityUnknownRole },
  { WebAccessibility::ROLE_BUTTON, NSAccessibilityButtonRole },
  { WebAccessibility::ROLE_RADIO_BUTTON, NSAccessibilityRadioButtonRole },
  { WebAccessibility::ROLE_CHECKBOX, NSAccessibilityCheckBoxRole },
  { WebAccessibility::ROLE_STATIC_TEXT, NSAccessibilityStaticTextRole},
  { WebAccessibility::ROLE_IMAGE, NSAccessibilityImageRole},
  { WebAccessibility::ROLE_TEXT_FIELD, NSAccessibilityTextFieldRole},
  { WebAccessibility::ROLE_TEXTAREA, NSAccessibilityTextAreaRole},
  { WebAccessibility::ROLE_LINK, NSAccessibilityLinkRole},
  { WebAccessibility::ROLE_SCROLLAREA, NSAccessibilityScrollAreaRole},
  { WebAccessibility::ROLE_SCROLLBAR, NSAccessibilityScrollBarRole},
  { WebAccessibility::ROLE_RADIO_GROUP, NSAccessibilityRadioGroupRole},
  { WebAccessibility::ROLE_TABLE, NSAccessibilityTableRole},
  { WebAccessibility::ROLE_TAB_GROUP, NSAccessibilityTabGroupRole},
  { WebAccessibility::ROLE_IGNORED, NSAccessibilityUnknownRole},
  { WebAccessibility::ROLE_WEB_AREA, @"AXWebArea"},
  { WebAccessibility::ROLE_GROUP, NSAccessibilityGroupRole},
  { WebAccessibility::ROLE_GRID, NSAccessibilityGridRole},
  { WebAccessibility::ROLE_WEBCORE_LINK, NSAccessibilityLinkRole},
};

bool GetState(WebAccessibility accessibility, int state) {
  return ((accessibility.state >> state) & 1);
}

} // anonymous namespace

@implementation BrowserAccessibility

- (id)initWithObject:(const WebAccessibility)accessibility
            delegate:(id<BrowserAccessibilityDelegate>)delegate
              parent:(id)parent {
  if ((self = [super init])) {
    webAccessibility_ = accessibility;
    parent_ = parent;
    delegate_ = delegate;
  }
  return self;
}

- (NSArray*)children {
  if (!children_.get()) {
    const std::vector<WebAccessibility>& accessibilityChildren =
        webAccessibility_.children;
    children_.reset(
        [[NSMutableArray alloc]
            initWithCapacity:accessibilityChildren.size()]);
    std::vector<WebAccessibility>::const_iterator iterator;
    for (iterator = accessibilityChildren.begin();
         iterator != accessibilityChildren.end();
         iterator++) {
      BrowserAccessibility* child =
          [[BrowserAccessibility alloc]
              initWithObject:*iterator
                    delegate:delegate_
                      parent:self];
      [child autorelease];
      [children_ addObject:child];
    }
  }
  return children_;
}

- (BOOL)isIgnored {
  return webAccessibility_.role == WebAccessibility::ROLE_IGNORED;
}

- (NSPoint)origin {
  return NSMakePoint(webAccessibility_.location.x,
                     webAccessibility_.location.y);
}

- (NSString*)role {
  NSString* role = NSAccessibilityUnknownRole;
  WebAccessibility::Role value = webAccessibility_.role;
  const size_t numRoles = sizeof(roles) / sizeof(roles[0]);
  for (size_t i = 0; i < numRoles; ++i) {
    if (roles[i].value == value) {
      role = roles[i].string;
      break;
    }
  }
  return role;
}

- (NSSize)size {
  return NSMakeSize(webAccessibility_.location.width,
                    webAccessibility_.location.height);
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
  if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return [self role];
  } else if ([attribute isEqualToString:NSAccessibilityDescriptionAttribute]) {
    return NSStringForWebAccessibilityAttribute(webAccessibility_.attributes,
        WebAccessibility::ATTR_DESCRIPTION);
  } else if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) {
    return [NSValue valueWithPoint:[delegate_ accessibilityPointInScreen:self]];
  } else if ([attribute isEqualToString:NSAccessibilitySizeAttribute]) {
    return [NSValue valueWithSize:[self size]];
  } else if (
      [attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute] ||
      [attribute isEqualToString:NSAccessibilityWindowAttribute]) {
    return [delegate_ window];
  } else if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return [self children];
  } else if ([attribute isEqualToString:NSAccessibilityParentAttribute]) {
    if (parent_) {
      return NSAccessibilityUnignoredAncestor(parent_);
    }
  } else if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) {
    return base::SysUTF16ToNSString(webAccessibility_.name);
  } else if ([attribute isEqualToString:NSAccessibilityHelpAttribute]) {
    return NSStringForWebAccessibilityAttribute(webAccessibility_.attributes,
                                                WebAccessibility::ATTR_HELP);
  } else if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    return base::SysUTF16ToNSString(webAccessibility_.value);
  } else if (
      [attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
    return NSAccessibilityRoleDescription([self role], nil);
  } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    NSNumber* ret = [NSNumber numberWithBool:
        GetState(webAccessibility_, WebAccessibility::STATE_FOCUSED)];
    return ret;
  } else if ([attribute isEqualToString:NSAccessibilityEnabledAttribute] ||
      [attribute isEqualToString:@"AXVisited"] ||
      [attribute isEqualToString:@"AXLoaded"]) {
    return [NSNumber numberWithBool:YES];
  }
  return nil;
}

- (NSArray *)accessibilityActionNames {
  return [NSArray arrayWithObjects:
      NSAccessibilityPressAction, NSAccessibilityShowMenuAction, nil];
}

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute
                                         index:(NSUInteger)index
                                      maxCount:(NSUInteger)maxCount {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  if (!fullArray)
    return nil;
  NSUInteger arrayCount = [fullArray count];
  if (index >= arrayCount)
    return nil;
  NSRange subRange;
  if ((index + maxCount) > arrayCount) {
    subRange = NSMakeRange(index, arrayCount - index);
  } else {
    subRange = NSMakeRange(index, maxCount);
  }
  return [fullArray subarrayWithRange:subRange];
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

- (NSArray *)accessibilityAttributeNames {
  return [NSArray arrayWithObjects:
      NSAccessibilityRoleAttribute,
      NSAccessibilityRoleDescriptionAttribute,
      NSAccessibilityChildrenAttribute,
      NSAccessibilityHelpAttribute,
      NSAccessibilityParentAttribute,
      NSAccessibilityPositionAttribute,
      NSAccessibilitySizeAttribute,
      NSAccessibilityTitleAttribute,
      NSAccessibilityDescriptionAttribute,
      NSAccessibilityValueAttribute,
      NSAccessibilityFocusedAttribute,
      NSAccessibilityEnabledAttribute,
      NSAccessibilityWindowAttribute,
      NSAccessibilityTopLevelUIElementAttribute,
      nil];
}

- (id)accessibilityFocusedUIElement {
  return self;
}

- (NSUInteger)accessibilityIndexOfChild:(id)child {
  NSUInteger index = 0;
  for (BrowserAccessibility* childToCheck in [self children]) {
    if ([child isEqual:childToCheck])
      return index;
    if (![childToCheck isIgnored])
      ++index;
  }
  return NSNotFound;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute {
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    return GetState(webAccessibility_, WebAccessibility::STATE_FOCUSABLE);
  } else if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    return !GetState(webAccessibility_, WebAccessibility::STATE_READONLY);
  }
  return NO;
}

- (BOOL)accessibilityIsIgnored {
  return [self isIgnored];
}

- (void)accessibilityPerformAction:(NSString *)action {
  // TODO(feldstein): Support more actions.
  [delegate_ doDefaultAction:webAccessibility_.id];
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  return NSAccessibilityActionDescription(action);
}

- (BOOL)accessibilitySetOverrideValue:(id)value
                         forAttribute:(NSString *)attribute {
  return NO;
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute {
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    NSNumber* focusedNumber = value;
    BOOL focused = [focusedNumber intValue];
    [delegate_ setAccessibilityFocus:focused
                     accessibilityId:webAccessibility_.id];
  }
}

- (id)accessibilityHitTest:(NSPoint)point {
  id hit = self;
  for (id child in [self children]) {
    NSPoint origin = [child origin];
    NSSize size = [child size];
    NSRect rect;
    rect.origin = origin;
    rect.size = size;
    if (NSPointInRect(point, rect)) {
      hit = child;
      id childResult = [child accessibilityHitTest:point];
      if (![childResult accessibilityIsIgnored]) {
        hit = childResult;
        break;
      }
    }
  }
  return NSAccessibilityUnignoredAncestor(hit);
}

- (BOOL)isEqual:(id)object {
  if (![object isKindOfClass:[BrowserAccessibility class]])
    return NO;
  return ([self hash] == [object hash]);
}

- (NSUInteger)hash {
  return webAccessibility_.id;
}

@end

