// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <execinfo.h>

#import "chrome/browser/cocoa/browser_accessibility.h"

#include "app/l10n_util_mac.h"
#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "grit/webkit_strings.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"

using webkit_glue::WebAccessibility;

namespace {

// Returns an autoreleased copy of the WebAccessibility's attribute.
NSString* NSStringForWebAccessibilityAttribute(
    const std::map<int32, string16>& attributes,
    WebAccessibility::Attribute attribute) {
  std::map<int32, string16>::const_iterator iter =
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

// GetState checks the bitmask used in webaccessibility.h to check
// if the given state was set on the accessibility object.
bool GetState(WebAccessibility accessibility, int state) {
  return ((accessibility.state >> state) & 1);
}

} // namespace

@implementation BrowserAccessibility

- (id)initWithObject:(const WebAccessibility&)accessibility
            delegate:(id<BrowserAccessibilityDelegate>)delegate
              parent:(id)parent {
  if ((self = [super init])) {
    webAccessibility_ = accessibility;
    parent_ = parent;
    delegate_ = delegate;
  }
  return self;
}

// Returns an array of BrowserAccessibility objects, representing the
// accessibility children of this object.
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
         ++iterator) {
      scoped_nsobject<BrowserAccessibility> child(
          [[BrowserAccessibility alloc]
              initWithObject:*iterator
                    delegate:delegate_
                      parent:self]);
      [children_ addObject:child];
    }
  }
  return children_;
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  return webAccessibility_.role == WebAccessibility::ROLE_IGNORED;
}

// The origin of this accessibility object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
- (NSPoint)origin {
  return NSMakePoint(webAccessibility_.location.x,
                     webAccessibility_.location.y);
}

// Returns a string indicating the role of this object.
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

// Returns a string indicating the role description of this object.
- (NSString*)roleDescription {
  // The following descriptions are specific to webkit.
  if ([[self role] isEqualToString:@"AXWebArea"])
    return l10n_util::GetNSString(IDS_AX_ROLE_WEB_AREA);

  if ([[self role] isEqualToString:@"NSAccessibilityLinkRole"])
    return l10n_util::GetNSString(IDS_AX_ROLE_LINK);

  if ([[self role] isEqualToString:@"AXHeading"])
    return l10n_util::GetNSString(IDS_AX_ROLE_HEADING);

  return NSAccessibilityRoleDescription([self role], nil);
}

// Returns the size of this object.
- (NSSize)size {
  return NSMakeSize(webAccessibility_.location.width,
                    webAccessibility_.location.height);
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return [self role];
  }
  if ([attribute isEqualToString:NSAccessibilityDescriptionAttribute]) {
    return NSStringForWebAccessibilityAttribute(webAccessibility_.attributes,
        WebAccessibility::ATTR_DESCRIPTION);
  }
  if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) {
    return [NSValue valueWithPoint:[delegate_ accessibilityPointInScreen:self]];
  }
  if ([attribute isEqualToString:NSAccessibilitySizeAttribute]) {
    return [NSValue valueWithSize:[self size]];
  }
  if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute] ||
      [attribute isEqualToString:NSAccessibilityWindowAttribute]) {
    return [delegate_ window];
  }
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return [self children];
  }
  if ([attribute isEqualToString:NSAccessibilityParentAttribute]) {
    if (parent_) {
      return NSAccessibilityUnignoredAncestor(parent_);
    }
  }
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) {
    return base::SysUTF16ToNSString(webAccessibility_.name);
  }
  if ([attribute isEqualToString:NSAccessibilityHelpAttribute]) {
    return NSStringForWebAccessibilityAttribute(webAccessibility_.attributes,
                                                WebAccessibility::ATTR_HELP);
  }
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    return base::SysUTF16ToNSString(webAccessibility_.value);
  }
  if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
    return [self roleDescription];
  }
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    NSNumber* ret = [NSNumber numberWithBool:
        GetState(webAccessibility_, WebAccessibility::STATE_FOCUSED)];
    return ret;
  }
  if ([attribute isEqualToString:NSAccessibilityEnabledAttribute] ||
      [attribute isEqualToString:@"AXVisited"] ||
      [attribute isEqualToString:@"AXLoaded"]) {
    return [NSNumber numberWithBool:YES];
  }
  return nil;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  return [NSArray arrayWithObjects:
      NSAccessibilityPressAction, NSAccessibilityShowMenuAction, nil];
}

// Returns a sub-array of values for the given attribute value, starting at
// index, with up to maxCount items.  If the given index is out of bounds,
// or there are no values for the given attribute, it will return nil.
// This method is used for querying subsets of values, without having to
// return a large set of data, such as elements with a large number of
// children.
- (NSArray*)accessibilityArrayAttributeValues:(NSString*)attribute
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

// Returns the count of the specified accessibility array attribute.
- (NSUInteger)accessibilityArrayAttributeCount:(NSString*)attribute {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

// Returns the list of accessibility attributes that this object supports.
- (NSArray*)accessibilityAttributeNames {
  return [NSArray arrayWithObjects:
      NSAccessibilityChildrenAttribute,
      NSAccessibilityDescriptionAttribute,
      NSAccessibilityEnabledAttribute,
      NSAccessibilityFocusedAttribute,
      NSAccessibilityHelpAttribute,
      NSAccessibilityParentAttribute,
      NSAccessibilityPositionAttribute,
      NSAccessibilityRoleAttribute,
      NSAccessibilityRoleDescriptionAttribute,
      NSAccessibilitySizeAttribute,
      NSAccessibilityTitleAttribute,
      NSAccessibilityTopLevelUIElementAttribute,
      NSAccessibilityValueAttribute,
      NSAccessibilityWindowAttribute,
      nil];
}

// Returns the deepest child that has user focus.
// TODO(feldstein): This does not work yet.
- (id)accessibilityFocusedUIElement {
  return self;
}

// Returns the index of the child in this objects array of children.
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

// Returns whether or not the specified attribute can be set by the
// accessibility API via |accessibilitySetValue:forAttribute:|.
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return GetState(webAccessibility_, WebAccessibility::STATE_FOCUSABLE);
  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return !GetState(webAccessibility_, WebAccessibility::STATE_READONLY);
  return NO;
}

// Returns whether or not this object should be ignored in the accessibilty
// tree.
- (BOOL)accessibilityIsIgnored {
  return [self isIgnored];
}

// Performs the given accessibilty action on the webkit accessibility object
// that backs this object.
- (void)accessibilityPerformAction:(NSString*)action {
  // TODO(feldstein): Support more actions.
  [delegate_ doDefaultAction:webAccessibility_.id];
}

// Returns the description of the given action.
- (NSString*)accessibilityActionDescription:(NSString*)action {
  return NSAccessibilityActionDescription(action);
}

// Sets an override value for a specific accessibility attribute.
// This class does not support this.
- (BOOL)accessibilitySetOverrideValue:(id)value
                         forAttribute:(NSString*)attribute {
  return NO;
}

// Sets the value for an accessibility attribute via the accessibility API.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    NSNumber* focusedNumber = value;
    BOOL focused = [focusedNumber intValue];
    [delegate_ setAccessibilityFocus:focused
                     accessibilityId:webAccessibility_.id];
  }
}

// Returns the deepest accessibility child that should not be ignored.
// It is assumed that the hit test has been narrowed down to this object
// or one of its children, so this will never return nil.
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

