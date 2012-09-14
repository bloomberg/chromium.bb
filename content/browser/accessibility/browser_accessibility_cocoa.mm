// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <execinfo.h>

#import "content/browser/accessibility/browser_accessibility_cocoa.h"

#include <map>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/common/content_client.h"
#include "grit/webkit_strings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"

// See http://openradar.appspot.com/9896491. This SPI has been tested on 10.5,
// 10.6, and 10.7. It allows accessibility clients to observe events posted on
// this object.
extern "C" void NSAccessibilityUnregisterUniqueIdForUIElement(id element);

using content::AccessibilityNodeData;
typedef AccessibilityNodeData::StringAttribute StringAttribute;

namespace {

// Returns an autoreleased copy of the AccessibilityNodeData's attribute.
NSString* NSStringForStringAttribute(
    const std::map<StringAttribute, string16>& attributes,
    StringAttribute attribute) {
  std::map<StringAttribute, string16>::const_iterator iter =
      attributes.find(attribute);
  NSString* returnValue = @"";
  if (iter != attributes.end()) {
    returnValue = base::SysUTF16ToNSString(iter->second);
  }
  return returnValue;
}

struct MapEntry {
  AccessibilityNodeData::Role webKitValue;
  NSString* nativeValue;
};

typedef std::map<AccessibilityNodeData::Role, NSString*> RoleMap;

// GetState checks the bitmask used in AccessibilityNodeData to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, int state) {
  return ((accessibility->state() >> state) & 1);
}

RoleMap BuildRoleMap() {
  const MapEntry roles[] = {
    { AccessibilityNodeData::ROLE_ALERT, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_ALERT_DIALOG, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_ANNOTATION, NSAccessibilityUnknownRole },
    { AccessibilityNodeData::ROLE_APPLICATION, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_ARTICLE, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_BROWSER, NSAccessibilityBrowserRole },
    { AccessibilityNodeData::ROLE_BUSY_INDICATOR,
        NSAccessibilityBusyIndicatorRole },
    { AccessibilityNodeData::ROLE_BUTTON, NSAccessibilityButtonRole },
    { AccessibilityNodeData::ROLE_CANVAS, NSAccessibilityImageRole },
    { AccessibilityNodeData::ROLE_CANVAS_WITH_FALLBACK_CONTENT,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_CELL, @"AXCell" },
    { AccessibilityNodeData::ROLE_CHECKBOX, NSAccessibilityCheckBoxRole },
    { AccessibilityNodeData::ROLE_COLOR_WELL, NSAccessibilityColorWellRole },
    { AccessibilityNodeData::ROLE_COLUMN, NSAccessibilityColumnRole },
    { AccessibilityNodeData::ROLE_COLUMN_HEADER, @"AXCell" },
    { AccessibilityNodeData::ROLE_DEFINITION_LIST_DEFINITION,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_DEFINITION_LIST_TERM,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_DIALOG, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_DIRECTORY, NSAccessibilityListRole },
    { AccessibilityNodeData::ROLE_DISCLOSURE_TRIANGLE,
        NSAccessibilityDisclosureTriangleRole },
    { AccessibilityNodeData::ROLE_DOCUMENT, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_DRAWER, NSAccessibilityDrawerRole },
    { AccessibilityNodeData::ROLE_EDITABLE_TEXT, NSAccessibilityTextFieldRole },
    { AccessibilityNodeData::ROLE_FOOTER, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_FORM, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_GRID, NSAccessibilityGridRole },
    { AccessibilityNodeData::ROLE_GROUP, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_GROW_AREA, NSAccessibilityGrowAreaRole },
    { AccessibilityNodeData::ROLE_HEADING, @"AXHeading" },
    { AccessibilityNodeData::ROLE_HELP_TAG, NSAccessibilityHelpTagRole },
    { AccessibilityNodeData::ROLE_HORIZONTAL_RULE, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_IGNORED, NSAccessibilityUnknownRole },
    { AccessibilityNodeData::ROLE_IMAGE, NSAccessibilityImageRole },
    { AccessibilityNodeData::ROLE_IMAGE_MAP, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_IMAGE_MAP_LINK, NSAccessibilityLinkRole },
    { AccessibilityNodeData::ROLE_INCREMENTOR, NSAccessibilityIncrementorRole },
    { AccessibilityNodeData::ROLE_LABEL, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LANDMARK_APPLICATION,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LANDMARK_BANNER, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LANDMARK_COMPLEMENTARY,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LANDMARK_CONTENTINFO,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LANDMARK_MAIN, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LANDMARK_NAVIGATION,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LANDMARK_SEARCH, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LINK, NSAccessibilityLinkRole },
    { AccessibilityNodeData::ROLE_LIST, NSAccessibilityListRole },
    { AccessibilityNodeData::ROLE_LIST_ITEM, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LIST_MARKER, @"AXListMarker" },
    { AccessibilityNodeData::ROLE_LISTBOX, NSAccessibilityListRole },
    { AccessibilityNodeData::ROLE_LISTBOX_OPTION, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_LOG, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_MARQUEE, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_MATH, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_MATTE, NSAccessibilityMatteRole },
    { AccessibilityNodeData::ROLE_MENU, NSAccessibilityMenuRole },
    { AccessibilityNodeData::ROLE_MENU_ITEM, NSAccessibilityMenuItemRole },
    { AccessibilityNodeData::ROLE_MENU_BUTTON, NSAccessibilityButtonRole },
    { AccessibilityNodeData::ROLE_MENU_LIST_OPTION,
        NSAccessibilityMenuItemRole },
    { AccessibilityNodeData::ROLE_MENU_LIST_POPUP, NSAccessibilityUnknownRole },
    { AccessibilityNodeData::ROLE_NOTE, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_OUTLINE, NSAccessibilityOutlineRole },
    { AccessibilityNodeData::ROLE_PARAGRAPH, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_POPUP_BUTTON,
        NSAccessibilityPopUpButtonRole },
    { AccessibilityNodeData::ROLE_PRESENTATIONAL, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_PROGRESS_INDICATOR,
        NSAccessibilityProgressIndicatorRole },
    { AccessibilityNodeData::ROLE_RADIO_BUTTON,
        NSAccessibilityRadioButtonRole },
    { AccessibilityNodeData::ROLE_RADIO_GROUP, NSAccessibilityRadioGroupRole },
    { AccessibilityNodeData::ROLE_REGION, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_ROOT_WEB_AREA, @"AXWebArea" },
    { AccessibilityNodeData::ROLE_ROW, NSAccessibilityRowRole },
    { AccessibilityNodeData::ROLE_ROW_HEADER, @"AXCell" },
    { AccessibilityNodeData::ROLE_RULER, NSAccessibilityRulerRole },
    { AccessibilityNodeData::ROLE_RULER_MARKER,
        NSAccessibilityRulerMarkerRole },
    // TODO(dtseng): we don't correctly support the attributes for these roles.
    // { AccessibilityNodeData::ROLE_SCROLLAREA,
    //    NSAccessibilityScrollAreaRole },
    { AccessibilityNodeData::ROLE_SCROLLBAR, NSAccessibilityScrollBarRole },
    { AccessibilityNodeData::ROLE_SHEET, NSAccessibilitySheetRole },
    { AccessibilityNodeData::ROLE_SLIDER, NSAccessibilitySliderRole },
    { AccessibilityNodeData::ROLE_SLIDER_THUMB, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_SPIN_BUTTON, NSAccessibilitySliderRole },
    { AccessibilityNodeData::ROLE_SPLITTER, NSAccessibilitySplitterRole },
    { AccessibilityNodeData::ROLE_SPLIT_GROUP, NSAccessibilitySplitGroupRole },
    { AccessibilityNodeData::ROLE_STATIC_TEXT, NSAccessibilityStaticTextRole },
    { AccessibilityNodeData::ROLE_STATUS, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_SYSTEM_WIDE, NSAccessibilityUnknownRole },
    { AccessibilityNodeData::ROLE_TAB, NSAccessibilityRadioButtonRole },
    { AccessibilityNodeData::ROLE_TAB_LIST, NSAccessibilityTabGroupRole },
    { AccessibilityNodeData::ROLE_TAB_PANEL, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_TABLE, NSAccessibilityTableRole },
    { AccessibilityNodeData::ROLE_TABLE_HEADER_CONTAINER,
        NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_TAB_GROUP_UNUSED,
        NSAccessibilityTabGroupRole },
    { AccessibilityNodeData::ROLE_TEXTAREA, NSAccessibilityTextAreaRole },
    { AccessibilityNodeData::ROLE_TEXT_FIELD, NSAccessibilityTextFieldRole },
    { AccessibilityNodeData::ROLE_TIMER, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_TOGGLE_BUTTON, NSAccessibilityButtonRole },
    { AccessibilityNodeData::ROLE_TOOLBAR, NSAccessibilityToolbarRole },
    { AccessibilityNodeData::ROLE_TOOLTIP, NSAccessibilityGroupRole },
    { AccessibilityNodeData::ROLE_TREE, NSAccessibilityOutlineRole },
    { AccessibilityNodeData::ROLE_TREE_GRID, NSAccessibilityTableRole },
    { AccessibilityNodeData::ROLE_TREE_ITEM, NSAccessibilityRowRole },
    { AccessibilityNodeData::ROLE_VALUE_INDICATOR,
        NSAccessibilityValueIndicatorRole },
    { AccessibilityNodeData::ROLE_WEBCORE_LINK, NSAccessibilityLinkRole },
    { AccessibilityNodeData::ROLE_WEB_AREA, @"AXWebArea" },
    { AccessibilityNodeData::ROLE_WINDOW, NSAccessibilityUnknownRole },
  };

  RoleMap role_map;
  for (size_t i = 0; i < arraysize(roles); ++i)
    role_map[roles[i].webKitValue] = roles[i].nativeValue;
  return role_map;
}

// A mapping of webkit roles to native roles.
NSString* NativeRoleFromAccessibilityNodeDataRole(
    const AccessibilityNodeData::Role& role) {
  CR_DEFINE_STATIC_LOCAL(RoleMap, web_accessibility_to_native_role,
                         (BuildRoleMap()));
  RoleMap::iterator it = web_accessibility_to_native_role.find(role);
  if (it != web_accessibility_to_native_role.end())
    return it->second;
  else
    return NSAccessibilityUnknownRole;
}

RoleMap BuildSubroleMap() {
  const MapEntry subroles[] = {
    { AccessibilityNodeData::ROLE_ALERT, @"AXApplicationAlert" },
    { AccessibilityNodeData::ROLE_ALERT_DIALOG, @"AXApplicationAlertDialog" },
    { AccessibilityNodeData::ROLE_ARTICLE, @"AXDocumentArticle" },
    { AccessibilityNodeData::ROLE_DEFINITION_LIST_DEFINITION, @"AXDefinition" },
    { AccessibilityNodeData::ROLE_DEFINITION_LIST_TERM, @"AXTerm" },
    { AccessibilityNodeData::ROLE_DIALOG, @"AXApplicationDialog" },
    { AccessibilityNodeData::ROLE_DOCUMENT, @"AXDocument" },
    { AccessibilityNodeData::ROLE_FOOTER, @"AXLandmarkContentInfo" },
    { AccessibilityNodeData::ROLE_LANDMARK_APPLICATION,
        @"AXLandmarkApplication" },
    { AccessibilityNodeData::ROLE_LANDMARK_BANNER, @"AXLandmarkBanner" },
    { AccessibilityNodeData::ROLE_LANDMARK_COMPLEMENTARY,
        @"AXLandmarkComplementary" },
    { AccessibilityNodeData::ROLE_LANDMARK_CONTENTINFO,
        @"AXLandmarkContentInfo" },
    { AccessibilityNodeData::ROLE_LANDMARK_MAIN, @"AXLandmarkMain" },
    { AccessibilityNodeData::ROLE_LANDMARK_NAVIGATION,
        @"AXLandmarkNavigation" },
    { AccessibilityNodeData::ROLE_LANDMARK_SEARCH, @"AXLandmarkSearch" },
    { AccessibilityNodeData::ROLE_LOG, @"AXApplicationLog" },
    { AccessibilityNodeData::ROLE_MARQUEE, @"AXApplicationMarquee" },
    { AccessibilityNodeData::ROLE_MATH, @"AXDocumentMath" },
    { AccessibilityNodeData::ROLE_NOTE, @"AXDocumentNote" },
    { AccessibilityNodeData::ROLE_REGION, @"AXDocumentRegion" },
    { AccessibilityNodeData::ROLE_STATUS, @"AXApplicationStatus" },
    { AccessibilityNodeData::ROLE_TAB_PANEL, @"AXTabPanel" },
    { AccessibilityNodeData::ROLE_TIMER, @"AXApplicationTimer" },
    { AccessibilityNodeData::ROLE_TOOLTIP, @"AXUserInterfaceTooltip" },
    { AccessibilityNodeData::ROLE_TREE_ITEM, NSAccessibilityOutlineRowSubrole },
  };

  RoleMap subrole_map;
  for (size_t i = 0; i < arraysize(subroles); ++i)
    subrole_map[subroles[i].webKitValue] = subroles[i].nativeValue;
  return subrole_map;
}

// A mapping of webkit roles to native subroles.
NSString* NativeSubroleFromAccessibilityNodeDataRole(
    const AccessibilityNodeData::Role& role) {
  CR_DEFINE_STATIC_LOCAL(RoleMap, web_accessibility_to_native_subrole,
                         (BuildSubroleMap()));
  RoleMap::iterator it = web_accessibility_to_native_subrole.find(role);
  if (it != web_accessibility_to_native_subrole.end())
    return it->second;
  else
    return nil;
}

// A mapping from an accessibility attribute to its method name.
NSDictionary* attributeToMethodNameMap = nil;

} // namespace

@implementation BrowserAccessibilityCocoa

+ (void)initialize {
  const struct {
    NSString* attribute;
    NSString* methodName;
  } attributeToMethodNameContainer[] = {
    { NSAccessibilityChildrenAttribute, @"children" },
    { NSAccessibilityColumnsAttribute, @"columns" },
    { NSAccessibilityDescriptionAttribute, @"description" },
    { NSAccessibilityEnabledAttribute, @"enabled" },
    { NSAccessibilityFocusedAttribute, @"focused" },
    { NSAccessibilityHelpAttribute, @"help" },
    { NSAccessibilityMaxValueAttribute, @"maxValue" },
    { NSAccessibilityMinValueAttribute, @"minValue" },
    { NSAccessibilityNumberOfCharactersAttribute, @"numberOfCharacters" },
    { NSAccessibilityOrientationAttribute, @"orientation" },
    { NSAccessibilityParentAttribute, @"parent" },
    { NSAccessibilityPositionAttribute, @"position" },
    { NSAccessibilityRoleAttribute, @"role" },
    { NSAccessibilityRoleDescriptionAttribute, @"roleDescription" },
    { NSAccessibilityRowsAttribute, @"rows" },
    { NSAccessibilitySizeAttribute, @"size" },
    { NSAccessibilitySubroleAttribute, @"subrole" },
    { NSAccessibilityTabsAttribute, @"tabs" },
    { NSAccessibilityTitleAttribute, @"title" },
    { NSAccessibilityTitleUIElementAttribute, @"titleUIElement" },
    { NSAccessibilityTopLevelUIElementAttribute, @"window" },
    { NSAccessibilityURLAttribute, @"url" },
    { NSAccessibilityValueAttribute, @"value" },
    { NSAccessibilityValueDescriptionAttribute, @"valueDescription" },
    { NSAccessibilityVisibleCharacterRangeAttribute, @"visibleCharacterRange" },
    { NSAccessibilityWindowAttribute, @"window" },
    { @"AXAccessKey", @"accessKey" },
    { @"AXARIAAtomic", @"ariaAtomic" },
    { @"AXARIABusy", @"ariaBusy" },
    { @"AXARIALive", @"ariaLive" },
    { @"AXARIARelevant", @"ariaRelevant" },
    { @"AXLoaded", @"loaded" },
    { @"AXLoadingProgress", @"loadingProgress" },
    { @"AXRequired", @"required" },
    { @"AXVisited", @"visited" },
  };

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  const size_t numAttributes = sizeof(attributeToMethodNameContainer) /
                               sizeof(attributeToMethodNameContainer[0]);
  for (size_t i = 0; i < numAttributes; ++i) {
    [dict setObject:attributeToMethodNameContainer[i].methodName
             forKey:attributeToMethodNameContainer[i].attribute];
  }
  attributeToMethodNameMap = dict;
  dict = nil;
}

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
    NSAccessibilityUnregisterUniqueIdForUIElement(self);
    delete browserAccessibility_;
    browserAccessibility_ = NULL;
  }

  [super dealloc];
}

- (NSString*)accessKey {
  return NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      AccessibilityNodeData::ATTR_ACCESS_KEY);
}

- (NSNumber*)ariaAtomic {
  bool boolValue = false;
  browserAccessibility_->GetBoolAttribute(
      AccessibilityNodeData::ATTR_LIVE_ATOMIC, &boolValue);
  return [NSNumber numberWithBool:boolValue];
}

- (NSNumber*)ariaBusy {
  bool boolValue = false;
  browserAccessibility_->GetBoolAttribute(
      AccessibilityNodeData::ATTR_LIVE_BUSY, &boolValue);
  return [NSNumber numberWithBool:boolValue];
}

- (NSString*)ariaLive {
  return NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      AccessibilityNodeData::ATTR_LIVE_STATUS);
}

- (NSString*)ariaRelevant {
  return NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      AccessibilityNodeData::ATTR_LIVE_RELEVANT);
}

// Returns an array of BrowserAccessibilityCocoa objects, representing the
// accessibility children of this object.
- (NSArray*)children {
  if (!children_.get()) {
    children_.reset([[NSMutableArray alloc]
        initWithCapacity:browserAccessibility_->child_count()] );
    for (uint32 index = 0;
         index < browserAccessibility_->child_count();
         ++index) {
      BrowserAccessibilityCocoa* child =
          browserAccessibility_->GetChild(index)->ToBrowserAccessibilityCocoa();
      if ([child isIgnored])
        [children_ addObjectsFromArray:[child children]];
      else
        [children_ addObject:child];
    }

    // Also, add indirect children (if any).
    for (uint32 i = 0;
         i < browserAccessibility_->indirect_child_ids().size();
         ++i) {
      int32 child_id = browserAccessibility_->indirect_child_ids()[i];
      BrowserAccessibility* child =
          browserAccessibility_->manager()->GetFromRendererID(child_id);

      // This only became necessary as a result of crbug.com/93095. It should be
      // a DCHECK in the future.
      if (child) {
        BrowserAccessibilityCocoa* child_cocoa =
            child->ToBrowserAccessibilityCocoa();
        [children_ addObject:child_cocoa];
      }
    }
  }
  return children_;
}

- (void)childrenChanged {
  if (![self isIgnored]) {
    children_.reset();
  } else {
    [browserAccessibility_->parent()->ToBrowserAccessibilityCocoa()
       childrenChanged];
  }
}

- (NSArray*)columns {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  for (BrowserAccessibilityCocoa* child in [self children]) {
    if ([[child role] isEqualToString:NSAccessibilityColumnRole])
      [ret addObject:child];
  }
  return ret;
}

- (NSString*)description {
  const std::map<StringAttribute, string16>& attributes =
      browserAccessibility_->string_attributes();
  std::map<StringAttribute, string16>::const_iterator iter =
      attributes.find(AccessibilityNodeData::ATTR_DESCRIPTION);
  if (iter != attributes.end())
    return base::SysUTF16ToNSString(iter->second);

  // If the role is anything other than an image, or if there's
  // a title or title UI element, just return an empty string.
  if (![[self role] isEqualToString:NSAccessibilityImageRole])
    return @"";
  if (!browserAccessibility_->name().empty())
    return @"";
  if ([self titleUIElement])
    return @"";

  // The remaining case is an image where there's no other title.
  // Return the base part of the filename as the description.
  iter = attributes.find(AccessibilityNodeData::ATTR_URL);
  if (iter != attributes.end()) {
    string16 filename = iter->second;
    // Given a url like http://foo.com/bar/baz.png, just return the
    // base name, e.g., "baz.png".
    size_t leftIndex = filename.size();
    while (leftIndex > 0 && filename[leftIndex - 1] != '/')
      leftIndex--;
    string16 basename = filename.substr(leftIndex);

    return base::SysUTF16ToNSString(basename);
  }

  return @"";
}

- (NSNumber*)enabled {
  return [NSNumber numberWithBool:
      !GetState(browserAccessibility_,
                AccessibilityNodeData::STATE_UNAVAILABLE)];
}

- (NSNumber*)focused {
  BrowserAccessibilityManager* manager = browserAccessibility_->manager();
  NSNumber* ret = [NSNumber numberWithBool:
      manager->GetFocus(NULL) == browserAccessibility_];
  return ret;
}

- (NSString*)help {
  return NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      AccessibilityNodeData::ATTR_HELP);
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  return [[self role] isEqualToString:NSAccessibilityUnknownRole];
}

- (NSNumber*)loaded {
  return [NSNumber numberWithBool:YES];
}

- (NSNumber*)loadingProgress {
  float floatValue = 0.0;
  browserAccessibility_->GetFloatAttribute(
      AccessibilityNodeData::ATTR_DOC_LOADING_PROGRESS, &floatValue);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)maxValue {
  float floatValue = 0.0;
  browserAccessibility_->GetFloatAttribute(
      AccessibilityNodeData::ATTR_MAX_VALUE_FOR_RANGE, &floatValue);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)minValue {
  float floatValue = 0.0;
  browserAccessibility_->GetFloatAttribute(
      AccessibilityNodeData::ATTR_MIN_VALUE_FOR_RANGE, &floatValue);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSString*)orientation {
  // We present a spin button as a vertical slider, with a role description
  // of "spin button".
  AccessibilityNodeData::Role internal_role =
      static_cast<AccessibilityNodeData::Role>(browserAccessibility_->role());
  if (internal_role == AccessibilityNodeData::ROLE_SPIN_BUTTON)
    return NSAccessibilityVerticalOrientationValue;

  if (GetState(browserAccessibility_, AccessibilityNodeData::STATE_VERTICAL))
    return NSAccessibilityVerticalOrientationValue;
  else
    return NSAccessibilityHorizontalOrientationValue;
}

- (NSNumber*)numberOfCharacters {
  return [NSNumber numberWithInt:browserAccessibility_->value().length()];
}

// The origin of this accessibility object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
- (NSPoint)origin {
  gfx::Rect bounds = browserAccessibility_->GetLocalBoundsRect();
  return NSMakePoint(bounds.x(), bounds.y());
}

- (id)parent {
  // A nil parent means we're the root.
  if (browserAccessibility_->parent()) {
    return NSAccessibilityUnignoredAncestor(
        browserAccessibility_->parent()->ToBrowserAccessibilityCocoa());
  } else {
    // Hook back up to RenderWidgetHostViewCocoa.
    return browserAccessibility_->manager()->GetParentView();
  }
}

- (NSValue*)position {
  return [NSValue valueWithPoint:[delegate_ accessibilityPointInScreen:self]];
}

- (NSNumber*)required {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, AccessibilityNodeData::STATE_REQUIRED)];
}

// Returns a string indicating the role of this object.
- (NSString*)role {
  AccessibilityNodeData::Role browserAccessibilityRole =
      static_cast<AccessibilityNodeData::Role>( browserAccessibility_->role());

  // Roles that we only determine at runtime.
  return NativeRoleFromAccessibilityNodeDataRole(browserAccessibilityRole);
}

// Returns a string indicating the role description of this object.
- (NSString*)roleDescription {
  NSString* role = [self role];

  content::ContentClient* content_client = content::GetContentClient();

  // The following descriptions are specific to webkit.
  if ([role isEqualToString:@"AXWebArea"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_WEB_AREA));
  }

  if ([role isEqualToString:@"NSAccessibilityLinkRole"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_LINK));
  }

  if ([role isEqualToString:@"AXHeading"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_HEADING));
  }

  if ([role isEqualToString:NSAccessibilityGroupRole] ||
      [role isEqualToString:NSAccessibilityRadioButtonRole]) {
    const std::vector<std::pair<string16, string16> >& htmlAttributes =
        browserAccessibility_->html_attributes();
    AccessibilityNodeData::Role browserAccessibilityRole =
        static_cast<AccessibilityNodeData::Role>(browserAccessibility_->role());

    if ((browserAccessibilityRole != AccessibilityNodeData::ROLE_GROUP &&
         browserAccessibilityRole != AccessibilityNodeData::ROLE_LIST_ITEM) ||
         browserAccessibilityRole == AccessibilityNodeData::ROLE_TAB) {
      for (size_t i = 0; i < htmlAttributes.size(); ++i) {
        const std::pair<string16, string16>& htmlAttribute = htmlAttributes[i];
        if (htmlAttribute.first == ASCIIToUTF16("role")) {
          // TODO(dtseng): This is not localized; see crbug/84814.
          return base::SysUTF16ToNSString(htmlAttribute.second);
        }
      }
    }
  }

  AccessibilityNodeData::Role internal_role =
      static_cast<AccessibilityNodeData::Role>(browserAccessibility_->role());
  switch(internal_role) {
  case AccessibilityNodeData::ROLE_FOOTER:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_FOOTER));
  case AccessibilityNodeData::ROLE_SPIN_BUTTON:
    // This control is similar to what VoiceOver calls a "stepper".
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_STEPPER));
  default:
    break;
  }

  return NSAccessibilityRoleDescription(role, nil);
}

- (NSArray*)rows {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  for (BrowserAccessibilityCocoa* child in [self children]) {
    if ([[child role] isEqualToString:NSAccessibilityRowRole])
      [ret addObject:child];
  }

  return ret;
}

// Returns the size of this object.
- (NSValue*)size {
  gfx::Rect bounds = browserAccessibility_->GetLocalBoundsRect();
  return  [NSValue valueWithSize:NSMakeSize(bounds.width(), bounds.height())];
}

// Returns a subrole based upon the role.
- (NSString*) subrole {
  AccessibilityNodeData::Role browserAccessibilityRole =
      static_cast<AccessibilityNodeData::Role>(browserAccessibility_->role());
  if (browserAccessibilityRole == AccessibilityNodeData::ROLE_TEXT_FIELD &&
      GetState(browserAccessibility_, AccessibilityNodeData::STATE_PROTECTED)) {
    return @"AXSecureTextField";
  }

  NSString* htmlTag = NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      AccessibilityNodeData::ATTR_HTML_TAG);

  if (browserAccessibilityRole == AccessibilityNodeData::ROLE_LIST) {
    if ([htmlTag isEqualToString:@"ul"] ||
        [htmlTag isEqualToString:@"ol"]) {
      return @"AXContentList";
    } else if ([htmlTag isEqualToString:@"dl"]) {
      return @"AXDefinitionList";
    }
  }

  return NativeSubroleFromAccessibilityNodeDataRole(browserAccessibilityRole);
}

// Returns all tabs in this subtree.
- (NSArray*)tabs {
  NSMutableArray* tabSubtree = [[[NSMutableArray alloc] init] autorelease];

  if (browserAccessibility_->role() == AccessibilityNodeData::ROLE_TAB)
    [tabSubtree addObject:self];

  for (uint i=0; i < [[self children] count]; ++i) {
    NSArray* tabChildren = [[[self children] objectAtIndex:i] tabs];
    if ([tabChildren count] > 0)
      [tabSubtree addObjectsFromArray:tabChildren];
  }

  return tabSubtree;
}

- (NSString*)title {
  return base::SysUTF16ToNSString(browserAccessibility_->name());
}

- (id)titleUIElement {
  int titleElementId;
  if (browserAccessibility_->GetIntAttribute(
          AccessibilityNodeData::ATTR_TITLE_UI_ELEMENT, &titleElementId)) {
    BrowserAccessibility* titleElement =
        browserAccessibility_->manager()->GetFromRendererID(titleElementId);
    if (titleElement)
      return titleElement->ToBrowserAccessibilityCocoa();
  }
  return nil;
}

- (NSString*)url {
  StringAttribute urlAttribute =
      [[self role] isEqualToString:@"AXWebArea"] ?
          AccessibilityNodeData::ATTR_DOC_URL :
          AccessibilityNodeData::ATTR_URL;
  return NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      urlAttribute);
}

- (id)value {
  // WebCore uses an attachmentView to get the below behavior.
  // We do not have any native views backing this object, so need
  // to approximate Cocoa ax behavior best as we can.
  NSString* role = [self role];
  if ([role isEqualToString:@"AXHeading"]) {
    NSString* headingLevel =
        NSStringForStringAttribute(
            browserAccessibility_->string_attributes(),
            AccessibilityNodeData::ATTR_HTML_TAG);
    if ([headingLevel length] >= 2) {
      return [NSNumber numberWithInt:
          [[headingLevel substringFromIndex:1] intValue]];
    }
  } else if ([role isEqualToString:NSAccessibilityButtonRole]) {
    // AXValue does not make sense for pure buttons.
    return @"";
  } else if ([role isEqualToString:NSAccessibilityCheckBoxRole] ||
             [role isEqualToString:NSAccessibilityRadioButtonRole]) {
    int value = 0;
    value = GetState(
        browserAccessibility_, AccessibilityNodeData::STATE_CHECKED) ? 1 : 0;
    value = GetState(
        browserAccessibility_, AccessibilityNodeData::STATE_SELECTED) ?
            1 :
            value;

    bool mixed = false;
    browserAccessibility_->GetBoolAttribute(
        AccessibilityNodeData::ATTR_BUTTON_MIXED, &mixed);
    if (mixed)
      value = 2;
    return [NSNumber numberWithInt:value];
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole]) {
    float floatValue;
    if (browserAccessibility_->GetFloatAttribute(
            AccessibilityNodeData::ATTR_VALUE_FOR_RANGE, &floatValue)) {
      return [NSNumber numberWithFloat:floatValue];
    }
  }

  return base::SysUTF16ToNSString(browserAccessibility_->value());
}

- (NSString*)valueDescription {
  if (!browserAccessibility_->value().empty())
    return base::SysUTF16ToNSString(browserAccessibility_->value());
  else
    return nil;
}

- (NSValue*)visibleCharacterRange {
  return [NSValue valueWithRange:
      NSMakeRange(0, browserAccessibility_->value().length())];
}

- (NSNumber*)visited {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, AccessibilityNodeData::STATE_TRAVERSED)];
}

- (id)window {
  return [delegate_ window];
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  SEL selector =
      NSSelectorFromString([attributeToMethodNameMap objectForKey:attribute]);
  if (selector)
    return [self performSelector:selector];

  // TODO(dtseng): refactor remaining attributes.
  int selStart, selEnd;
  if (browserAccessibility_->GetIntAttribute(
          AccessibilityNodeData::ATTR_TEXT_SEL_START, &selStart) &&
      browserAccessibility_->
          GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_END, &selEnd)) {
    if (selStart > selEnd)
      std::swap(selStart, selEnd);
    int selLength = selEnd - selStart;
    if ([attribute isEqualToString:
        NSAccessibilityInsertionPointLineNumberAttribute]) {
      const std::vector<int32>& line_breaks =
          browserAccessibility_->line_breaks();
      for (int i = 0; i < static_cast<int>(line_breaks.size()); ++i) {
        if (line_breaks[i] > selStart)
          return [NSNumber numberWithInt:i];
      }
      return [NSNumber numberWithInt:static_cast<int>(line_breaks.size())];
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

// Returns the accessibility value for the given attribute and parameter. If the
// value isn't supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  const std::vector<int32>& line_breaks = browserAccessibility_->line_breaks();
  int len = static_cast<int>(browserAccessibility_->value().size());

  if ([attribute isEqualToString:
      NSAccessibilityStringForRangeParameterizedAttribute]) {
    NSRange range = [(NSValue*)parameter rangeValue];
    return base::SysUTF16ToNSString(
        browserAccessibility_->value().substr(range.location, range.length));
  }

  if ([attribute isEqualToString:
      NSAccessibilityLineForIndexParameterizedAttribute]) {
    int index = [(NSNumber*)parameter intValue];
    for (int i = 0; i < static_cast<int>(line_breaks.size()); ++i) {
      if (line_breaks[i] > index)
        return [NSNumber numberWithInt:i];
    }
    return [NSNumber numberWithInt:static_cast<int>(line_breaks.size())];
  }

  if ([attribute isEqualToString:
      NSAccessibilityRangeForLineParameterizedAttribute]) {
    int line_index = [(NSNumber*)parameter intValue];
    int line_count = static_cast<int>(line_breaks.size()) + 1;
    if (line_index < 0 || line_index >= line_count)
      return nil;
    int start = line_index > 0 ? line_breaks[line_index - 1] : 0;
    int end = line_index < line_count - 1 ? line_breaks[line_index] : len;
    return [NSValue valueWithRange:
        NSMakeRange(start, end - start)];
  }

  // TODO(dtseng): support the following attributes.
  if ([attribute isEqualTo:
          NSAccessibilityRangeForPositionParameterizedAttribute] ||
      [attribute isEqualTo:
          NSAccessibilityRangeForIndexParameterizedAttribute] ||
      [attribute isEqualTo:
          NSAccessibilityBoundsForRangeParameterizedAttribute] ||
      [attribute isEqualTo:NSAccessibilityRTFForRangeParameterizedAttribute] ||
      [attribute isEqualTo:
          NSAccessibilityStyleRangeForIndexParameterizedAttribute]) {
    return nil;
  }
  return nil;
}

// Returns an array of parameterized attributes names that this object will
// respond to.
- (NSArray*)accessibilityParameterizedAttributeNames {
  if ([[self role] isEqualToString:NSAccessibilityTextFieldRole] ||
      [[self role] isEqualToString:NSAccessibilityTextAreaRole]) {
    return [NSArray arrayWithObjects:
        NSAccessibilityLineForIndexParameterizedAttribute,
        NSAccessibilityRangeForLineParameterizedAttribute,
        NSAccessibilityStringForRangeParameterizedAttribute,
        NSAccessibilityRangeForPositionParameterizedAttribute,
        NSAccessibilityRangeForIndexParameterizedAttribute,
        NSAccessibilityBoundsForRangeParameterizedAttribute,
        NSAccessibilityRTFForRangeParameterizedAttribute,
        NSAccessibilityAttributedStringForRangeParameterizedAttribute,
        NSAccessibilityStyleRangeForIndexParameterizedAttribute,
        nil];
  }
  return nil;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  NSMutableArray* ret =
      [NSMutableArray arrayWithObject:NSAccessibilityShowMenuAction];
  NSString* role = [self role];
  // TODO(dtseng): this should only get set when there's a default action.
  if (![role isEqualToString:NSAccessibilityStaticTextRole] &&
      ![role isEqualToString:NSAccessibilityTextAreaRole] &&
      ![role isEqualToString:NSAccessibilityTextFieldRole]) {
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
  // General attributes.
  NSMutableArray* ret = [NSMutableArray arrayWithObjects:
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
      NSAccessibilitySubroleAttribute,
      NSAccessibilityTitleAttribute,
      NSAccessibilityTopLevelUIElementAttribute,
      NSAccessibilityValueAttribute,
      NSAccessibilityWindowAttribute,
      NSAccessibilityURLAttribute,
      @"AXAccessKey",
      @"AXRequired",
      @"AXVisited",
      nil];

  // Specific role attributes.
  NSString* role = [self role];
  if ([role isEqualToString:NSAccessibilityTableRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityColumnsAttribute,
        NSAccessibilityRowsAttribute,
        nil]];
  } else if ([role isEqualToString:@"AXWebArea"]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXLoaded",
        @"AXLoadingProgress",
        nil]];
  } else if ([role isEqualToString:NSAccessibilityTextFieldRole] ||
             [role isEqualToString:NSAccessibilityTextAreaRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityInsertionPointLineNumberAttribute,
        NSAccessibilityNumberOfCharactersAttribute,
        NSAccessibilitySelectedTextAttribute,
        NSAccessibilitySelectedTextRangeAttribute,
        NSAccessibilityVisibleCharacterRangeAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityTabGroupRole]) {
    [ret addObject:NSAccessibilityTabsAttribute];
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityMaxValueAttribute,
        NSAccessibilityMinValueAttribute,
        NSAccessibilityOrientationAttribute,
        NSAccessibilityValueDescriptionAttribute,
        nil]];
  }

  // Live regions.
  string16 s;
  if (browserAccessibility_->GetStringAttribute(
          AccessibilityNodeData::ATTR_LIVE_STATUS, &s)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIALive",
        @"AXARIARelevant",
        nil]];
  }
  if (browserAccessibility_->GetStringAttribute(
          AccessibilityNodeData::ATTR_CONTAINER_LIVE_STATUS, &s)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIAAtomic",
        @"AXARIABusy",
        nil]];
  }

  // Title UI Element.
  int i;
  if (browserAccessibility_->GetIntAttribute(
          AccessibilityNodeData::ATTR_TITLE_UI_ELEMENT, &i)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
         NSAccessibilityTitleUIElementAttribute,
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
    return GetState(browserAccessibility_,
        AccessibilityNodeData::STATE_FOCUSABLE);
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    bool canSetValue = false;
    browserAccessibility_->GetBoolAttribute(
        AccessibilityNodeData::ATTR_CAN_SET_VALUE, &canSetValue);
    return canSetValue;
  }
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] &&
      ([[self role] isEqualToString:NSAccessibilityTextFieldRole] ||
       [[self role] isEqualToString:NSAccessibilityTextAreaRole]))
    return YES;

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
  if ([action isEqualToString:NSAccessibilityPressAction])
    [delegate_ doDefaultAction:browserAccessibility_->renderer_id()];
  else if ([action isEqualToString:NSAccessibilityShowMenuAction])
    [delegate_ performShowMenuAction:self];
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
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    NSRange range = [(NSValue*)value rangeValue];
    [delegate_
        accessibilitySetTextSelection:browserAccessibility_->renderer_id()
        startOffset:range.location
        endOffset:range.location + range.length];
  }
}

// Returns the deepest accessibility child that should not be ignored.
// It is assumed that the hit test has been narrowed down to this object
// or one of its children, so this will never return nil.
- (id)accessibilityHitTest:(NSPoint)point {
  BrowserAccessibilityCocoa* hit = self;
  for (BrowserAccessibilityCocoa* child in [self children]) {
    NSPoint origin = [child origin];
    NSSize size = [[child size] sizeValue];
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

- (BOOL)accessibilityShouldUseUniqueId {
  return YES;
}

@end

