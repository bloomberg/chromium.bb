// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <execinfo.h>

#import "chrome/browser/accessibility/browser_accessibility_cocoa.h"

#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "grit/webkit_strings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "ui/base/l10n/l10n_util_mac.h"

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
  { WebAccessibility::ROLE_CHECKBOX, NSAccessibilityCheckBoxRole },
  { WebAccessibility::ROLE_COLUMN, NSAccessibilityColumnRole },
  { WebAccessibility::ROLE_GRID, NSAccessibilityGridRole },
  { WebAccessibility::ROLE_GROUP, NSAccessibilityGroupRole },
  { WebAccessibility::ROLE_HEADING, @"AXHeading" },
  { WebAccessibility::ROLE_IGNORED, NSAccessibilityUnknownRole },
  { WebAccessibility::ROLE_IMAGE, NSAccessibilityImageRole },
  { WebAccessibility::ROLE_LINK, NSAccessibilityLinkRole },
  { WebAccessibility::ROLE_LIST, NSAccessibilityListRole },
  { WebAccessibility::ROLE_RADIO_BUTTON, NSAccessibilityRadioButtonRole },
  { WebAccessibility::ROLE_RADIO_GROUP, NSAccessibilityRadioGroupRole },
  { WebAccessibility::ROLE_ROW, NSAccessibilityRowRole },
  { WebAccessibility::ROLE_SCROLLAREA, NSAccessibilityScrollAreaRole },
  { WebAccessibility::ROLE_SCROLLBAR, NSAccessibilityScrollBarRole },
  { WebAccessibility::ROLE_STATIC_TEXT, NSAccessibilityStaticTextRole },
  { WebAccessibility::ROLE_TABLE, NSAccessibilityTableRole },
  { WebAccessibility::ROLE_TAB_GROUP, NSAccessibilityTabGroupRole },
  { WebAccessibility::ROLE_TEXT_FIELD, NSAccessibilityTextFieldRole },
  { WebAccessibility::ROLE_TEXTAREA, NSAccessibilityTextAreaRole },
  { WebAccessibility::ROLE_WEB_AREA, @"AXWebArea" },
  { WebAccessibility::ROLE_WEBCORE_LINK, NSAccessibilityLinkRole },
};

// GetState checks the bitmask used in webaccessibility.h to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, int state) {
  return ((accessibility->state() >> state) & 1);
}

} // namespace

@implementation BrowserAccessibilityCocoa

- (id)initWithObject:(BrowserAccessibility*)accessibility
            delegate:(id<BrowserAccessibilityDelegateCocoa>)delegate {
  if ((self = [super init])) {
    browserAccessibility_ = accessibility;
    delegate_ = delegate;
  }
  return self;
}

// Deletes our associated BrowserAccessibilityMac.
- (void)dealloc {
  if (browserAccessibility_) {
    delete browserAccessibility_;
    browserAccessibility_ = NULL;
  }

  [super dealloc];
}

// Returns an array of BrowserAccessibilityCocoa objects, representing the
// accessibility children of this object.
- (NSArray*)children {
  if (!children_.get()) {
    children_.reset([[NSMutableArray alloc]
        initWithCapacity:browserAccessibility_->GetChildCount()] );
    for (uint32 index = 0;
         index < browserAccessibility_->GetChildCount();
         ++index) {
      BrowserAccessibilityCocoa* child =
          browserAccessibility_->GetChild(index)->toBrowserAccessibilityCocoa();
      if ([child isIgnored])
        [children_ addObjectsFromArray:[child children]];
      else
        [children_ addObject:child];
    }
  }
  return children_;
}

- (void)childrenChanged {
  if (![self isIgnored]) {
    children_.reset();
  } else {
    [browserAccessibility_->parent()->toBrowserAccessibilityCocoa()
       childrenChanged];
  }
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  return [self role] == NSAccessibilityUnknownRole;
}

// The origin of this accessibility object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
- (NSPoint)origin {
  return NSMakePoint(browserAccessibility_->location().x,
                     browserAccessibility_->location().y);
}

// Returns a string indicating the role of this object.
- (NSString*)role {
  WebAccessibility::Role value =
      static_cast<WebAccessibility::Role>( browserAccessibility_->role());

  // Roles that we only determine at runtime.
  if (value == WebAccessibility::ROLE_TEXT_FIELD &&
      GetState(browserAccessibility_, WebAccessibility::STATE_PROTECTED)) {
    return @"AXSecureTextField";
  }

  NSString* role = NSAccessibilityUnknownRole;
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
  return NSMakeSize(browserAccessibility_->location().width,
                    browserAccessibility_->location().height);
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return [self role];
  }
  if ([attribute isEqualToString:NSAccessibilityDescriptionAttribute]) {
    return NSStringForWebAccessibilityAttribute(
        browserAccessibility_->attributes(),
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
    // A nil parent means we're the root.
    if (browserAccessibility_->parent()) {
      return NSAccessibilityUnignoredAncestor(
          browserAccessibility_->parent()->toBrowserAccessibilityCocoa());
    } else {
      // Hook back up to RenderWidgetHostViewCocoa.
      return browserAccessibility_->manager()->GetParentView();
    }
  }
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) {
    return base::SysUTF16ToNSString(browserAccessibility_->name());
  }
  if ([attribute isEqualToString:NSAccessibilityHelpAttribute]) {
    return NSStringForWebAccessibilityAttribute(
        browserAccessibility_->attributes(),
        WebAccessibility::ATTR_HELP);
  }
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    // WebCore uses an attachmentView to get the below behavior.
    // We do not have any native views backing this object, so need
    // to approximate Cocoa ax behavior best as we can.
    if ([self role] == @"AXHeading") {
      NSString* headingLevel =
          NSStringForWebAccessibilityAttribute(
              browserAccessibility_->attributes(),
              WebAccessibility::ATTR_HTML_TAG);
      if ([headingLevel length] >= 2) {
        return [NSNumber numberWithInt:
            [[headingLevel substringFromIndex:1] intValue]];
      }
    } else if ([self role] == NSAccessibilityButtonRole) {
      // AXValue does not make sense for pure buttons.
      return @"";
    } else if ([self role] == NSAccessibilityCheckBoxRole) {
      return [NSNumber numberWithInt:GetState(
          browserAccessibility_, WebAccessibility::STATE_CHECKED) ? 1 : 0];
    } else {
      return base::SysUTF16ToNSString(browserAccessibility_->value());
    }
  }
  if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
    return [self roleDescription];
  }
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    NSNumber* ret = [NSNumber numberWithBool:
        GetState(browserAccessibility_, WebAccessibility::STATE_FOCUSED)];
    return ret;
  }
  if ([attribute isEqualToString:NSAccessibilityEnabledAttribute]) {
    return [NSNumber numberWithBool:
        !GetState(browserAccessibility_, WebAccessibility::STATE_UNAVAILABLE)];
  }
  if ([attribute isEqualToString:@"AXVisited"]) {
    return [NSNumber numberWithBool:
        GetState(browserAccessibility_, WebAccessibility::STATE_TRAVERSED)];
  }

  // AXWebArea attributes.
  if ([attribute isEqualToString:@"AXLoaded"])
    return [NSNumber numberWithBool:YES];
  if ([attribute isEqualToString:@"AXURL"]) {
    WebAccessibility::Attribute urlAttribute =
        [[self role] isEqualToString:@"AXWebArea"] ?
            WebAccessibility::ATTR_DOC_URL :
            WebAccessibility::ATTR_URL;
    return NSStringForWebAccessibilityAttribute(
        browserAccessibility_->attributes(),
        urlAttribute);
  }

  // Text related attributes.
  if ([attribute isEqualToString:
      NSAccessibilityNumberOfCharactersAttribute]) {
    return [NSNumber numberWithInt:browserAccessibility_->value().length()];
  }
  if ([attribute isEqualToString:
      NSAccessibilityVisibleCharacterRangeAttribute]) {
    return [NSValue valueWithRange:
        NSMakeRange(0, browserAccessibility_->value().length())];
  }

  int selStart, selEnd;
  if (browserAccessibility_->
          GetAttributeAsInt(WebAccessibility::ATTR_TEXT_SEL_START, &selStart) &&
      browserAccessibility_->
          GetAttributeAsInt(WebAccessibility::ATTR_TEXT_SEL_END, &selEnd)) {
    if (selStart > selEnd)
      std::swap(selStart, selEnd);
    int selLength = selEnd - selStart;
    if ([attribute isEqualToString:
        NSAccessibilityInsertionPointLineNumberAttribute]) {
      return [NSNumber numberWithInt:0];
    }
    if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
      return base::SysUTF16ToNSString(browserAccessibility_->value().substr(
          selStart, selLength));
    }
    if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
      return [NSValue valueWithRange:NSMakeRange(selStart, selLength)];
    }
  }
  return nil;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  // General actions.
  [ret addObject:NSAccessibilityShowMenuAction];

  // TODO(dtseng): this should only get set when there's a default action.
  if ([self role] != NSAccessibilityStaticTextRole &&
      [self role] != NSAccessibilityTextAreaRole &&
      [self role] != NSAccessibilityTextFieldRole) {
    [ret addObject:NSAccessibilityPressAction];
  }

  return ret;
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
  NSMutableArray* ret = [[NSMutableArray alloc] init];

  // General attributes.
  [ret addObjectsFromArray:[NSArray arrayWithObjects:
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
      @"AXURL",
      @"AXVisited",
      nil]];

  // Specific role attributes.
  if ([self role] == @"AXWebArea") {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXLoaded",
        nil]];
  }

  if ([self role] == NSAccessibilityTextFieldRole) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityInsertionPointLineNumberAttribute,
        NSAccessibilityNumberOfCharactersAttribute,
        NSAccessibilitySelectedTextAttribute,
        NSAccessibilitySelectedTextRangeAttribute,
        NSAccessibilityVisibleCharacterRangeAttribute,
        nil]];
  }
  return ret;
}

// Returns the index of the child in this objects array of children.
- (NSUInteger)accessibilityGetIndexOf:(id)child {
  NSUInteger index = 0;
  for (BrowserAccessibilityCocoa* childToCheck in [self children]) {
    if ([child isEqual:childToCheck])
      return index;
    ++index;
  }
  return NSNotFound;
}

// Returns whether or not the specified attribute can be set by the
// accessibility API via |accessibilitySetValue:forAttribute:|.
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return GetState(browserAccessibility_, WebAccessibility::STATE_FOCUSABLE);
  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return !GetState(browserAccessibility_, WebAccessibility::STATE_READONLY);
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
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    [delegate_ doDefaultAction:browserAccessibility_->renderer_id()];
  } else if ([action isEqualToString:NSAccessibilityShowMenuAction]) {
    // TODO(dtseng): implement.
  }
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
                     accessibilityId:browserAccessibility_->renderer_id()];
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
  if (![object isKindOfClass:[BrowserAccessibilityCocoa class]])
    return NO;
  return ([self hash] == [object hash]);
}

- (NSUInteger)hash {
  // Potentially called during dealloc.
  if (!browserAccessibility_)
    return [super hash];
  return browserAccessibility_->renderer_id();
}

@end

