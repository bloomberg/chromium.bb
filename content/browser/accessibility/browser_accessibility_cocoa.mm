// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"

// See http://openradar.appspot.com/9896491. This SPI has been tested on 10.5,
// 10.6, and 10.7. It allows accessibility clients to observe events posted on
// this object.
extern "C" void NSAccessibilityUnregisterUniqueIdForUIElement(id element);

typedef WebAccessibility::StringAttribute StringAttribute;

namespace {

// Returns an autoreleased copy of the WebAccessibility's attribute.
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
  WebAccessibility::Role webKitValue;
  NSString* nativeValue;
};

typedef std::map<WebAccessibility::Role, NSString*> RoleMap;

struct AttributeToMethodNameEntry {
  NSString* attribute;
  NSString* methodName;
};

const AttributeToMethodNameEntry attributeToMethodNameContainer[] = {
  { NSAccessibilityChildrenAttribute, @"children" },
  { NSAccessibilityColumnsAttribute, @"columns" },
  { NSAccessibilityDescriptionAttribute, @"description" },
  { NSAccessibilityEnabledAttribute, @"enabled" },
  { NSAccessibilityFocusedAttribute, @"focused" },
  { NSAccessibilityHelpAttribute, @"help" },
  { NSAccessibilityMaxValueAttribute, @"maxValue" },
  { NSAccessibilityMinValueAttribute, @"minValue" },
  { NSAccessibilityNumberOfCharactersAttribute, @"numberOfCharacters" },
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

// GetState checks the bitmask used in webaccessibility.h to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, int state) {
  return ((accessibility->state() >> state) & 1);
}

RoleMap BuildRoleMap() {
  const MapEntry roles[] = {
    { WebAccessibility::ROLE_ALERT, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_ALERT_DIALOG, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_ANNOTATION, NSAccessibilityUnknownRole },
    { WebAccessibility::ROLE_APPLICATION, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_ARTICLE, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_BROWSER, NSAccessibilityBrowserRole },
    { WebAccessibility::ROLE_BUSY_INDICATOR, NSAccessibilityBusyIndicatorRole },
    { WebAccessibility::ROLE_BUTTON, NSAccessibilityButtonRole },
    { WebAccessibility::ROLE_CELL, @"AXCell" },
    { WebAccessibility::ROLE_CHECKBOX, NSAccessibilityCheckBoxRole },
    { WebAccessibility::ROLE_COLOR_WELL, NSAccessibilityColorWellRole },
    { WebAccessibility::ROLE_COLUMN, NSAccessibilityColumnRole },
    { WebAccessibility::ROLE_COLUMN_HEADER, @"AXCell" },
    { WebAccessibility::ROLE_DEFINITION_LIST_DEFINITION,
        NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_DEFINITION_LIST_TERM, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_DIALOG, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_DIRECTORY, NSAccessibilityListRole },
    { WebAccessibility::ROLE_DISCLOSURE_TRIANGLE,
        NSAccessibilityDisclosureTriangleRole },
    { WebAccessibility::ROLE_DOCUMENT, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_DRAWER, NSAccessibilityDrawerRole },
    { WebAccessibility::ROLE_EDITABLE_TEXT, NSAccessibilityTextFieldRole },
    { WebAccessibility::ROLE_GRID, NSAccessibilityGridRole },
    { WebAccessibility::ROLE_GROUP, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_GROW_AREA, NSAccessibilityGrowAreaRole },
    { WebAccessibility::ROLE_HEADING, @"AXHeading" },
    { WebAccessibility::ROLE_HELP_TAG, NSAccessibilityHelpTagRole },
    { WebAccessibility::ROLE_IGNORED, NSAccessibilityUnknownRole },
    { WebAccessibility::ROLE_IMAGE, NSAccessibilityImageRole },
    { WebAccessibility::ROLE_IMAGE_MAP, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_IMAGE_MAP_LINK, NSAccessibilityLinkRole },
    { WebAccessibility::ROLE_INCREMENTOR, NSAccessibilityIncrementorRole },
    { WebAccessibility::ROLE_LANDMARK_APPLICATION, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LANDMARK_BANNER, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LANDMARK_COMPLEMENTARY, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LANDMARK_CONTENTINFO, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LANDMARK_MAIN, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LANDMARK_NAVIGATION, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LANDMARK_SEARCH, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LINK, NSAccessibilityLinkRole },
    { WebAccessibility::ROLE_LIST, NSAccessibilityListRole },
    { WebAccessibility::ROLE_LIST_ITEM, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LIST_MARKER, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LISTBOX, NSAccessibilityListRole },
    { WebAccessibility::ROLE_LISTBOX_OPTION, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_LOG, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_MARQUEE, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_MATH, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_MATTE, NSAccessibilityMatteRole },
    { WebAccessibility::ROLE_MENU, NSAccessibilityMenuRole },
    { WebAccessibility::ROLE_MENU_ITEM, NSAccessibilityMenuItemRole },
    { WebAccessibility::ROLE_MENU_BUTTON, NSAccessibilityButtonRole },
    { WebAccessibility::ROLE_MENU_LIST_OPTION, NSAccessibilityMenuItemRole },
    { WebAccessibility::ROLE_MENU_LIST_POPUP, NSAccessibilityUnknownRole },
    { WebAccessibility::ROLE_NOTE, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_OUTLINE, NSAccessibilityOutlineRole },
    { WebAccessibility::ROLE_POPUP_BUTTON, NSAccessibilityPopUpButtonRole },
    { WebAccessibility::ROLE_PROGRESS_INDICATOR,
        NSAccessibilityProgressIndicatorRole },
    { WebAccessibility::ROLE_RADIO_BUTTON, NSAccessibilityRadioButtonRole },
    { WebAccessibility::ROLE_RADIO_GROUP, NSAccessibilityRadioGroupRole },
    { WebAccessibility::ROLE_REGION, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_ROOT_WEB_AREA, @"AXWebArea" },
    { WebAccessibility::ROLE_ROW, NSAccessibilityRowRole },
    { WebAccessibility::ROLE_ROW_HEADER, @"AXCell" },
    { WebAccessibility::ROLE_RULER, NSAccessibilityRulerRole },
    { WebAccessibility::ROLE_RULER_MARKER, NSAccessibilityRulerMarkerRole },
    // TODO(dtseng): we don't correctly support the attributes for these roles.
    // { WebAccessibility::ROLE_SCROLLAREA, NSAccessibilityScrollAreaRole },
    // { WebAccessibility::ROLE_SCROLLBAR, NSAccessibilityScrollBarRole },
    { WebAccessibility::ROLE_SHEET, NSAccessibilitySheetRole },
    { WebAccessibility::ROLE_SLIDER, NSAccessibilitySliderRole },
    { WebAccessibility::ROLE_SLIDER_THUMB, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_SPLITTER, NSAccessibilitySplitterRole },
    { WebAccessibility::ROLE_SPLIT_GROUP, NSAccessibilitySplitGroupRole },
    { WebAccessibility::ROLE_STATIC_TEXT, NSAccessibilityStaticTextRole },
    { WebAccessibility::ROLE_STATUS, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_SYSTEM_WIDE, NSAccessibilityUnknownRole },
    { WebAccessibility::ROLE_TAB, NSAccessibilityRadioButtonRole },
    { WebAccessibility::ROLE_TAB_LIST, NSAccessibilityTabGroupRole },
    { WebAccessibility::ROLE_TAB_PANEL, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_TABLE, NSAccessibilityTableRole },
    { WebAccessibility::ROLE_TABLE_HEADER_CONTAINER, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_TAB_GROUP, NSAccessibilityTabGroupRole },
    { WebAccessibility::ROLE_TEXTAREA, NSAccessibilityTextAreaRole },
    { WebAccessibility::ROLE_TEXT_FIELD, NSAccessibilityTextFieldRole },
    { WebAccessibility::ROLE_TIMER, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_TOOLBAR, NSAccessibilityToolbarRole },
    { WebAccessibility::ROLE_TOOLTIP, NSAccessibilityGroupRole },
    { WebAccessibility::ROLE_TREE, NSAccessibilityOutlineRole },
    { WebAccessibility::ROLE_TREE_GRID, NSAccessibilityTableRole },
    { WebAccessibility::ROLE_TREE_ITEM, NSAccessibilityRowRole },
    { WebAccessibility::ROLE_VALUE_INDICATOR,
        NSAccessibilityValueIndicatorRole },
    { WebAccessibility::ROLE_WEBCORE_LINK, NSAccessibilityLinkRole },
    { WebAccessibility::ROLE_WEB_AREA, @"AXWebArea" },
    { WebAccessibility::ROLE_WINDOW, NSAccessibilityUnknownRole },
  };

  RoleMap role_map;
  for (size_t i = 0; i < arraysize(roles); ++i)
    role_map[roles[i].webKitValue] = roles[i].nativeValue;
  return role_map;
}

// A mapping of webkit roles to native roles.
NSString* NativeRoleFromWebAccessibilityRole(
    const WebAccessibility::Role& role) {
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
    { WebAccessibility::ROLE_ALERT, @"AXApplicationAlert" },
    { WebAccessibility::ROLE_ALERT_DIALOG, @"AXApplicationAlertDialog" },
    { WebAccessibility::ROLE_ARTICLE, @"AXDocumentArticle" },
    { WebAccessibility::ROLE_DEFINITION_LIST_DEFINITION, @"AXDefinition" },
    { WebAccessibility::ROLE_DEFINITION_LIST_TERM, @"AXTerm" },
    { WebAccessibility::ROLE_DIALOG, @"AXApplicationDialog" },
    { WebAccessibility::ROLE_DOCUMENT, @"AXDocument" },
    { WebAccessibility::ROLE_LANDMARK_APPLICATION, @"AXLandmarkApplication" },
    { WebAccessibility::ROLE_LANDMARK_BANNER, @"AXLandmarkBanner" },
    { WebAccessibility::ROLE_LANDMARK_COMPLEMENTARY,
        @"AXLandmarkComplementary" },
    { WebAccessibility::ROLE_LANDMARK_CONTENTINFO, @"AXLandmarkContentInfo" },
    { WebAccessibility::ROLE_LANDMARK_MAIN, @"AXLandmarkMain" },
    { WebAccessibility::ROLE_LANDMARK_NAVIGATION, @"AXLandmarkNavigation" },
    { WebAccessibility::ROLE_LANDMARK_SEARCH, @"AXLandmarkSearch" },
    { WebAccessibility::ROLE_LOG, @"AXApplicationLog" },
    { WebAccessibility::ROLE_MARQUEE, @"AXApplicationMarquee" },
    { WebAccessibility::ROLE_MATH, @"AXDocumentMath" },
    { WebAccessibility::ROLE_NOTE, @"AXDocumentNote" },
    { WebAccessibility::ROLE_REGION, @"AXDocumentRegion" },
    { WebAccessibility::ROLE_STATUS, @"AXApplicationStatus" },
    { WebAccessibility::ROLE_TAB_PANEL, @"AXTabPanel" },
    { WebAccessibility::ROLE_TIMER, @"AXApplicationTimer" },
    { WebAccessibility::ROLE_TOOLTIP, @"AXUserInterfaceTooltip" },
    { WebAccessibility::ROLE_TREE_ITEM, NSAccessibilityOutlineRowSubrole },
  };

  RoleMap subrole_map;
  for (size_t i = 0; i < arraysize(subroles); ++i)
    subrole_map[subroles[i].webKitValue] = subroles[i].nativeValue;
  return subrole_map;
}

// A mapping of webkit roles to native subroles.
NSString* NativeSubroleFromWebAccessibilityRole(
    const WebAccessibility::Role& role) {
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
      WebAccessibility::ATTR_ACCESS_KEY);
}

- (NSNumber*)ariaAtomic {
  bool boolValue = false;
  browserAccessibility_->GetBoolAttribute(
      WebAccessibility::ATTR_LIVE_ATOMIC, &boolValue);
  return [NSNumber numberWithBool:boolValue];
}

- (NSNumber*)ariaBusy {
  bool boolValue = false;
  browserAccessibility_->GetBoolAttribute(
      WebAccessibility::ATTR_LIVE_BUSY, &boolValue);
  return [NSNumber numberWithBool:boolValue];
}

- (NSString*)ariaLive {
  return NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      WebAccessibility::ATTR_LIVE_STATUS);
}

- (NSString*)ariaRelevant {
  return NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      WebAccessibility::ATTR_LIVE_RELEVANT);
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
          browserAccessibility_->GetChild(index)->toBrowserAccessibilityCocoa();
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
            child->toBrowserAccessibilityCocoa();
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
    [browserAccessibility_->parent()->toBrowserAccessibilityCocoa()
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
      attributes.find(WebAccessibility::ATTR_DESCRIPTION);
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
  iter = attributes.find(WebAccessibility::ATTR_URL);
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
      !GetState(browserAccessibility_, WebAccessibility::STATE_UNAVAILABLE)];
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
      WebAccessibility::ATTR_HELP);
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
      WebAccessibility::ATTR_DOC_LOADING_PROGRESS, &floatValue);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)maxValue {
  float floatValue = 0.0;
  browserAccessibility_->GetFloatAttribute(
      WebAccessibility::ATTR_MAX_VALUE_FOR_RANGE, &floatValue);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)minValue {
  float floatValue = 0.0;
  browserAccessibility_->GetFloatAttribute(
      WebAccessibility::ATTR_MIN_VALUE_FOR_RANGE, &floatValue);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)numberOfCharacters {
  return [NSNumber numberWithInt:browserAccessibility_->value().length()];
}

// The origin of this accessibility object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
- (NSPoint)origin {
  return NSMakePoint(browserAccessibility_->location().x(),
                     browserAccessibility_->location().y());
}

- (id)parent {
  // A nil parent means we're the root.
  if (browserAccessibility_->parent()) {
    return NSAccessibilityUnignoredAncestor(
        browserAccessibility_->parent()->toBrowserAccessibilityCocoa());
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
      GetState(browserAccessibility_, WebAccessibility::STATE_REQUIRED)];
}

// Returns a string indicating the role of this object.
- (NSString*)role {
  WebAccessibility::Role browserAccessibilityRole =
      static_cast<WebAccessibility::Role>( browserAccessibility_->role());

  // Roles that we only determine at runtime.
  return NativeRoleFromWebAccessibilityRole(browserAccessibilityRole);
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
    WebAccessibility::Role browserAccessibilityRole =
        static_cast<WebAccessibility::Role>(browserAccessibility_->role());

    if ((browserAccessibilityRole != WebAccessibility::ROLE_GROUP &&
         browserAccessibilityRole != WebAccessibility::ROLE_LIST_ITEM) ||
         browserAccessibilityRole == WebAccessibility::ROLE_TAB) {
      for (size_t i = 0; i < htmlAttributes.size(); ++i) {
        const std::pair<string16, string16>& htmlAttribute = htmlAttributes[i];
        if (htmlAttribute.first == ASCIIToUTF16("role")) {
          // TODO(dtseng): This is not localized; see crbug/84814.
          return base::SysUTF16ToNSString(htmlAttribute.second);
        }
      }
    }
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
  return  [NSValue valueWithSize:NSMakeSize(
      browserAccessibility_->location().width(),
      browserAccessibility_->location().height())];
}

// Returns a subrole based upon the role.
- (NSString*) subrole {
  WebAccessibility::Role browserAccessibilityRole =
      static_cast<WebAccessibility::Role>(browserAccessibility_->role());
  if (browserAccessibilityRole == WebAccessibility::ROLE_TEXT_FIELD &&
      GetState(browserAccessibility_, WebAccessibility::STATE_PROTECTED)) {
    return @"AXSecureTextField";
  }

  NSString* htmlTag = NSStringForStringAttribute(
      browserAccessibility_->string_attributes(),
      WebAccessibility::ATTR_HTML_TAG);

  if (browserAccessibilityRole == WebAccessibility::ROLE_LIST) {
    if ([htmlTag isEqualToString:@"ul"] ||
        [htmlTag isEqualToString:@"ol"]) {
      return @"AXContentList";
    } else if ([htmlTag isEqualToString:@"dl"]) {
      return @"AXDefinitionList";
    }
  }

  return NativeSubroleFromWebAccessibilityRole(browserAccessibilityRole);
}

// Returns all tabs in this subtree.
- (NSArray*)tabs {
  NSMutableArray* tabSubtree = [[[NSMutableArray alloc] init] autorelease];

  if (browserAccessibility_->role() == WebAccessibility::ROLE_TAB)
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
          WebAccessibility::ATTR_TITLE_UI_ELEMENT, &titleElementId)) {
    BrowserAccessibility* titleElement =
        browserAccessibility_->manager()->GetFromRendererID(titleElementId);
    if (titleElement)
      return titleElement->toBrowserAccessibilityCocoa();
  }
  return nil;
}

- (NSString*)url {
  StringAttribute urlAttribute =
      [[self role] isEqualToString:@"AXWebArea"] ?
          WebAccessibility::ATTR_DOC_URL :
          WebAccessibility::ATTR_URL;
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
            WebAccessibility::ATTR_HTML_TAG);
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
        browserAccessibility_, WebAccessibility::STATE_CHECKED) ? 1 : 0;
    value = GetState(
        browserAccessibility_, WebAccessibility::STATE_SELECTED) ? 1 : value;

    bool mixed = false;
    browserAccessibility_->GetBoolAttribute(
        WebAccessibility::ATTR_BUTTON_MIXED, &mixed);
    if (mixed)
      value = 2;
    return [NSNumber numberWithInt:value];
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole]) {
    float floatValue;
    if (browserAccessibility_->GetFloatAttribute(
            WebAccessibility::ATTR_VALUE_FOR_RANGE, &floatValue)) {
      return [NSNumber numberWithFloat:floatValue];
    }
  }

  return base::SysUTF16ToNSString(browserAccessibility_->value());
}

- (NSValue*)visibleCharacterRange {
  return [NSValue valueWithRange:
      NSMakeRange(0, browserAccessibility_->value().length())];
}

- (NSNumber*)visited {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, WebAccessibility::STATE_TRAVERSED)];
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
          WebAccessibility::ATTR_TEXT_SEL_START, &selStart) &&
      browserAccessibility_->
          GetIntAttribute(WebAccessibility::ATTR_TEXT_SEL_END, &selEnd)) {
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
        nil]];
  }

  // Live regions.
  string16 s;
  if (browserAccessibility_->GetStringAttribute(
          WebAccessibility::ATTR_LIVE_STATUS, &s)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIALive",
        @"AXARIARelevant",
        nil]];
  }
  if (browserAccessibility_->GetStringAttribute(
          WebAccessibility::ATTR_CONTAINER_LIVE_STATUS, &s)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIAAtomic",
        @"AXARIABusy",
        nil]];
  }

  // Title UI Element.
  int i;
  if (browserAccessibility_->GetIntAttribute(
          WebAccessibility::ATTR_TITLE_UI_ELEMENT, &i)) {
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
    return GetState(browserAccessibility_, WebAccessibility::STATE_FOCUSABLE);
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    bool canSetValue = false;
    browserAccessibility_->GetBoolAttribute(
        WebAccessibility::ATTR_CAN_SET_VALUE, &canSetValue);
    return canSetValue;
  }
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

