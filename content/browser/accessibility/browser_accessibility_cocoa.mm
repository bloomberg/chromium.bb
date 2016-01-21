// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <execinfo.h>
#include <stddef.h>
#include <stdint.h>

#import "content/browser/accessibility/browser_accessibility_cocoa.h"

#include <map>

#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/public/common/content_client.h"
#import "ui/accessibility/platform/ax_platform_node_mac.h"

// See http://openradar.appspot.com/9896491. This SPI has been tested on 10.5,
// 10.6, and 10.7. It allows accessibility clients to observe events posted on
// this object.
extern "C" void NSAccessibilityUnregisterUniqueIdForUIElement(id element);

using ui::AXNodeData;
using content::AccessibilityMatchPredicate;
using content::BrowserAccessibility;
using content::BrowserAccessibilityDelegate;
using content::BrowserAccessibilityManager;
using content::BrowserAccessibilityManagerMac;
using content::ContentClient;
using content::OneShotAccessibilityTreeSearch;
typedef ui::AXStringAttribute StringAttribute;

namespace {

// VoiceOver uses -1 to mean "no limit" for AXResultsLimit.
const int kAXResultsLimitNoLimit = -1;

// Returns an autoreleased copy of the AXNodeData's attribute.
NSString* NSStringForStringAttribute(
    BrowserAccessibility* browserAccessibility,
    StringAttribute attribute) {
  return base::SysUTF8ToNSString(
      browserAccessibility->GetStringAttribute(attribute));
}

// GetState checks the bitmask used in AXNodeData to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, ui::AXState state) {
  return ((accessibility->GetState() >> state) & 1);
}

// A mapping from an accessibility attribute to its method name.
NSDictionary* attributeToMethodNameMap = nil;

// Given a search key provided to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate, return a predicate that can be added
// to OneShotAccessibilityTreeSearch.
AccessibilityMatchPredicate PredicateForSearchKey(NSString* searchKey) {
  if ([searchKey isEqualToString:@"AXAnyTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return true;
    };
  } else if ([searchKey isEqualToString:@"AXBlockquoteSameLevelSearchKey"] ||
             [searchKey isEqualToString:@"AXBlockquoteSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      // TODO(dmazzoni): implement the "same level" part.
      return current->GetRole() == ui::AX_ROLE_BLOCKQUOTE;
    };
  } else if ([searchKey isEqualToString:@"AXBoldFontSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXButtonSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_BUTTON ||
              current->GetRole() == ui::AX_ROLE_MENU_BUTTON ||
              current->GetRole() == ui::AX_ROLE_POP_UP_BUTTON ||
              current->GetRole() == ui::AX_ROLE_SWITCH ||
              current->GetRole() == ui::AX_ROLE_TOGGLE_BUTTON);
    };
  } else if ([searchKey isEqualToString:@"AXCheckBoxSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_CHECK_BOX ||
              current->GetRole() == ui::AX_ROLE_MENU_ITEM_CHECK_BOX);
    };
  } else if ([searchKey isEqualToString:@"AXControlSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      if (current->IsControl())
        return true;
      if (current->HasState(ui::AX_STATE_FOCUSABLE) &&
          current->GetRole() != ui::AX_ROLE_IMAGE_MAP_LINK &&
          current->GetRole() != ui::AX_ROLE_LINK) {
        return true;
      }
      return false;
    };
  } else if ([searchKey isEqualToString:@"AXDifferentTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() != start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXFontChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFontColorChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFrameSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      if (current->IsWebAreaForPresentationalIframe())
        return false;
      if (!current->GetParent())
        return false;
      return (current->GetRole() == ui::AX_ROLE_WEB_AREA ||
              current->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA);
    };
  } else if ([searchKey isEqualToString:@"AXGraphicSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == ui::AX_ROLE_IMAGE;
    };
  } else if ([searchKey isEqualToString:@"AXHeadingLevel1SearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_HEADING &&
              current->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 1);
    };
  } else if ([searchKey isEqualToString:@"AXHeadingLevel2SearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_HEADING &&
              current->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 2);
    };
  } else if ([searchKey isEqualToString:@"AXHeadingLevel3SearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_HEADING &&
              current->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 3);
    };
  } else if ([searchKey isEqualToString:@"AXHeadingLevel4SearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_HEADING &&
              current->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 4);
    };
  } else if ([searchKey isEqualToString:@"AXHeadingLevel5SearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_HEADING &&
              current->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 5);
    };
  } else if ([searchKey isEqualToString:@"AXHeadingLevel6SearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_HEADING &&
              current->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) == 6);
    };
  } else if ([searchKey isEqualToString:@"AXHeadingSameLevelSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_HEADING &&
              start->GetRole() == ui::AX_ROLE_HEADING &&
              (current->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL) ==
               start->GetIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL)));
    };
  } else if ([searchKey isEqualToString:@"AXHeadingSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == ui::AX_ROLE_HEADING;
    };
  } else if ([searchKey isEqualToString:@"AXHighlightedSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXItalicFontSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXLandmarkSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_APPLICATION ||
              current->GetRole() == ui::AX_ROLE_BANNER ||
              current->GetRole() == ui::AX_ROLE_COMPLEMENTARY ||
              current->GetRole() == ui::AX_ROLE_CONTENT_INFO ||
              current->GetRole() == ui::AX_ROLE_FORM ||
              current->GetRole() == ui::AX_ROLE_MAIN ||
              current->GetRole() == ui::AX_ROLE_NAVIGATION ||
              current->GetRole() == ui::AX_ROLE_SEARCH);
    };
  } else if ([searchKey isEqualToString:@"AXLinkSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == ui::AX_ROLE_LINK;
    };
  } else if ([searchKey isEqualToString:@"AXListSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == ui::AX_ROLE_LIST;
    };
  } else if ([searchKey isEqualToString:@"AXLiveRegionSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->HasStringAttribute(ui::AX_ATTR_LIVE_STATUS);
    };
  } else if ([searchKey isEqualToString:@"AXMisspelledWordSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXOutlineSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == ui::AX_ROLE_TREE;
    };
  } else if ([searchKey isEqualToString:@"AXPlainTextSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXRadioGroupSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == ui::AX_ROLE_RADIO_GROUP;
    };
  } else if ([searchKey isEqualToString:@"AXSameTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXStaticTextSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->IsTextOnlyObject();
    };
  } else if ([searchKey isEqualToString:@"AXStyleChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXTableSameLevelSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      // TODO(dmazzoni): implement the "same level" part.
      return current->GetRole() == ui::AX_ROLE_GRID ||
             current->GetRole() == ui::AX_ROLE_TABLE;
    };
  } else if ([searchKey isEqualToString:@"AXTableSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == ui::AX_ROLE_GRID ||
             current->GetRole() == ui::AX_ROLE_TABLE;
    };
  } else if ([searchKey isEqualToString:@"AXTextFieldSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->IsSimpleTextControl() || current->IsRichTextControl();
    };
  } else if ([searchKey isEqualToString:@"AXUnderlineSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXUnvisitedLinkSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_LINK &&
              !current->HasState(ui::AX_STATE_VISITED));
    };
  } else if ([searchKey isEqualToString:@"AXVisitedLinkSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return (current->GetRole() == ui::AX_ROLE_LINK &&
              current->HasState(ui::AX_STATE_VISITED));
    };
  }

  return nullptr;
}

// Initialize a OneShotAccessibilityTreeSearch object given the parameters
// passed to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate. Return true on success.
bool InitializeAccessibilityTreeSearch(
    OneShotAccessibilityTreeSearch* search,
    id parameter) {
  if (![parameter isKindOfClass:[NSDictionary class]])
    return false;
  NSDictionary* dictionary = parameter;

  id startElementParameter = [dictionary objectForKey:@"AXStartElement"];
  if ([startElementParameter isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    BrowserAccessibilityCocoa* startNodeCocoa =
        (BrowserAccessibilityCocoa*)startElementParameter;
    search->SetStartNode([startNodeCocoa browserAccessibility]);
  }

  bool immediateDescendantsOnly = false;
  NSNumber *immediateDescendantsOnlyParameter =
      [dictionary objectForKey:@"AXImmediateDescendantsOnly"];
  if ([immediateDescendantsOnlyParameter isKindOfClass:[NSNumber class]])
    immediateDescendantsOnly = [immediateDescendantsOnlyParameter boolValue];

  bool visibleOnly = false;
  NSNumber *visibleOnlyParameter = [dictionary objectForKey:@"AXVisibleOnly"];
  if ([visibleOnlyParameter isKindOfClass:[NSNumber class]])
    visibleOnly = [visibleOnlyParameter boolValue];

  content::OneShotAccessibilityTreeSearch::Direction direction =
      content::OneShotAccessibilityTreeSearch::FORWARDS;
  NSString* directionParameter = [dictionary objectForKey:@"AXDirection"];
  if ([directionParameter isKindOfClass:[NSString class]]) {
    if ([directionParameter isEqualToString:@"AXDirectionNext"])
      direction = content::OneShotAccessibilityTreeSearch::FORWARDS;
    else if ([directionParameter isEqualToString:@"AXDirectionPrevious"])
      direction = content::OneShotAccessibilityTreeSearch::BACKWARDS;
  }

  int resultsLimit = kAXResultsLimitNoLimit;
  NSNumber* resultsLimitParameter = [dictionary objectForKey:@"AXResultsLimit"];
  if ([resultsLimitParameter isKindOfClass:[NSNumber class]])
    resultsLimit = [resultsLimitParameter intValue];

  std::string searchText;
  NSString* searchTextParameter = [dictionary objectForKey:@"AXSearchText"];
  if ([searchTextParameter isKindOfClass:[NSString class]])
    searchText = base::SysNSStringToUTF8(searchTextParameter);

  search->SetDirection(direction);
  search->SetImmediateDescendantsOnly(immediateDescendantsOnly);
  search->SetVisibleOnly(visibleOnly);
  search->SetSearchText(searchText);

  // Mac uses resultsLimit == -1 for unlimited, that that's
  // the default for OneShotAccessibilityTreeSearch already.
  // Only set the results limit if it's nonnegative.
  if (resultsLimit >= 0)
    search->SetResultLimit(resultsLimit);

  id searchKey = [dictionary objectForKey:@"AXSearchKey"];
  if ([searchKey isKindOfClass:[NSString class]]) {
    AccessibilityMatchPredicate predicate =
        PredicateForSearchKey((NSString*)searchKey);
    if (predicate)
      search->AddPredicate(predicate);
  } else if ([searchKey isKindOfClass:[NSArray class]]) {
    size_t searchKeyCount = static_cast<size_t>([searchKey count]);
    for (size_t i = 0; i < searchKeyCount; ++i) {
      id key = [searchKey objectAtIndex:i];
      if ([key isKindOfClass:[NSString class]]) {
        AccessibilityMatchPredicate predicate =
            PredicateForSearchKey((NSString*)key);
        if (predicate)
          search->AddPredicate(predicate);
      }
    }
  }

  return true;
}

} // namespace

@implementation BrowserAccessibilityCocoa

+ (void)initialize {
  const struct {
    NSString* attribute;
    NSString* methodName;
  } attributeToMethodNameContainer[] = {
    { NSAccessibilityChildrenAttribute, @"children" },
    { NSAccessibilityColumnsAttribute, @"columns" },
    { NSAccessibilityColumnHeaderUIElementsAttribute, @"columnHeaders" },
    { NSAccessibilityColumnIndexRangeAttribute, @"columnIndexRange" },
    { NSAccessibilityContentsAttribute, @"contents" },
    { NSAccessibilityDescriptionAttribute, @"description" },
    { NSAccessibilityDisclosingAttribute, @"disclosing" },
    { NSAccessibilityDisclosedByRowAttribute, @"disclosedByRow" },
    { NSAccessibilityDisclosureLevelAttribute, @"disclosureLevel" },
    { NSAccessibilityDisclosedRowsAttribute, @"disclosedRows" },
    { NSAccessibilityEnabledAttribute, @"enabled" },
    { NSAccessibilityExpandedAttribute, @"expanded" },
    { NSAccessibilityFocusedAttribute, @"focused" },
    { NSAccessibilityHeaderAttribute, @"header" },
    { NSAccessibilityHelpAttribute, @"help" },
    { NSAccessibilityIndexAttribute, @"index" },
    { NSAccessibilityLinkedUIElementsAttribute, @"linkedUIElements" },
    { NSAccessibilityMaxValueAttribute, @"maxValue" },
    { NSAccessibilityMinValueAttribute, @"minValue" },
    { NSAccessibilityNumberOfCharactersAttribute, @"numberOfCharacters" },
    { NSAccessibilityOrientationAttribute, @"orientation" },
    { NSAccessibilityParentAttribute, @"parent" },
    { NSAccessibilityPlaceholderValueAttribute, @"placeholderValue" },
    { NSAccessibilityPositionAttribute, @"position" },
    { NSAccessibilityRoleAttribute, @"role" },
    { NSAccessibilityRoleDescriptionAttribute, @"roleDescription" },
    { NSAccessibilityRowHeaderUIElementsAttribute, @"rowHeaders" },
    { NSAccessibilityRowIndexRangeAttribute, @"rowIndexRange" },
    { NSAccessibilityRowsAttribute, @"rows" },
    // TODO(aboxhall): expose NSAccessibilityServesAsTitleForUIElementsAttribute
    { NSAccessibilitySelectedChildrenAttribute, @"selectedChildren" },
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
    { NSAccessibilityVisibleCellsAttribute, @"visibleCells" },
    { NSAccessibilityVisibleChildrenAttribute, @"visibleChildren" },
    { NSAccessibilityVisibleColumnsAttribute, @"visibleColumns" },
    { NSAccessibilityVisibleRowsAttribute, @"visibleRows" },
    { NSAccessibilityWindowAttribute, @"window" },
    { @"AXAccessKey", @"accessKey" },
    { @"AXARIAAtomic", @"ariaAtomic" },
    { @"AXARIABusy", @"ariaBusy" },
    { @"AXARIALive", @"ariaLive" },
    { @"AXARIASetSize", @"ariaSetSize" },
    { @"AXARIAPosInSet", @"ariaPosInSet" },
    { @"AXARIARelevant", @"ariaRelevant" },
    { @"AXDropEffects", @"dropeffect" },
    { @"AXGrabbed", @"grabbed" },
    { @"AXInvalid", @"invalid" },
    { @"AXLoaded", @"loaded" },
    { @"AXLoadingProgress", @"loadingProgress" },
    { @"AXRequired", @"required" },
    { @"AXSortDirection", @"sortDirection" },
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

- (id)initWithObject:(BrowserAccessibility*)accessibility {
  if ((self = [super init]))
    browserAccessibility_ = accessibility;
  return self;
}

- (void)detach {
  if (browserAccessibility_) {
    NSAccessibilityUnregisterUniqueIdForUIElement(self);
    browserAccessibility_ = NULL;
  }
}

- (NSString*)accessKey {
  return NSStringForStringAttribute(
      browserAccessibility_, ui::AX_ATTR_ACCESS_KEY);
}

- (NSNumber*)ariaAtomic {
  bool boolValue = browserAccessibility_->GetBoolAttribute(
      ui::AX_ATTR_LIVE_ATOMIC);
  return [NSNumber numberWithBool:boolValue];
}

- (NSNumber*)ariaBusy {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, ui::AX_STATE_BUSY)];
}

- (NSString*)ariaLive {
  return NSStringForStringAttribute(
      browserAccessibility_, ui::AX_ATTR_LIVE_STATUS);
}

- (NSString*)ariaRelevant {
  return NSStringForStringAttribute(
      browserAccessibility_, ui::AX_ATTR_LIVE_RELEVANT);
}

- (NSNumber*)ariaPosInSet {
  return [NSNumber numberWithInt:
      browserAccessibility_->GetIntAttribute(ui::AX_ATTR_POS_IN_SET)];
}

- (NSNumber*)ariaSetSize {
  return [NSNumber numberWithInt:
      browserAccessibility_->GetIntAttribute(ui::AX_ATTR_SET_SIZE)];
}

// Returns an array of BrowserAccessibilityCocoa objects, representing the
// accessibility children of this object.
- (NSArray*)children {
  if (!children_) {
    uint32_t childCount = browserAccessibility_->PlatformChildCount();
    children_.reset([[NSMutableArray alloc] initWithCapacity:childCount]);
    for (uint32_t index = 0; index < childCount; ++index) {
      BrowserAccessibilityCocoa* child =
          browserAccessibility_->PlatformGetChild(index)->
              ToBrowserAccessibilityCocoa();
      if ([child isIgnored])
        [children_ addObjectsFromArray:[child children]];
      else
        [children_ addObject:child];
    }

    // Also, add indirect children (if any).
    const std::vector<int32_t>& indirectChildIds =
        browserAccessibility_->GetIntListAttribute(
            ui::AX_ATTR_INDIRECT_CHILD_IDS);
    for (uint32_t i = 0; i < indirectChildIds.size(); ++i) {
      int32_t child_id = indirectChildIds[i];
      BrowserAccessibility* child =
          browserAccessibility_->manager()->GetFromID(child_id);

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
    [browserAccessibility_->GetParent()->ToBrowserAccessibilityCocoa()
       childrenChanged];
  }
}

- (NSArray*)columnHeaders {
  if ([self internalRole] != ui::AX_ROLE_TABLE &&
      [self internalRole] != ui::AX_ROLE_GRID) {
    return nil;
  }

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  const std::vector<int32_t>& uniqueCellIds =
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_UNIQUE_CELL_IDS);
  for (size_t i = 0; i < uniqueCellIds.size(); ++i) {
    int id = uniqueCellIds[i];
    BrowserAccessibility* cell =
        browserAccessibility_->manager()->GetFromID(id);
    if (cell && cell->GetRole() == ui::AX_ROLE_COLUMN_HEADER)
      [ret addObject:cell->ToBrowserAccessibilityCocoa()];
  }
  return ret;
}

- (NSValue*)columnIndexRange {
  if (!browserAccessibility_->IsCellOrTableHeaderRole())
    return nil;

  int column = -1;
  int colspan = -1;
  browserAccessibility_->GetIntAttribute(
      ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX, &column);
  browserAccessibility_->GetIntAttribute(
      ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN, &colspan);
  if (column >= 0 && colspan >= 1)
    return [NSValue valueWithRange:NSMakeRange(column, colspan)];
  return nil;
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
  // Mac OS X wants static text exposed in AXValue.
  if ([self shouldExposeNameInAXValue])
    return @"";

  // If the name came from a single related element and it's present in the
  // tree, it will be exposed in AXTitleUIElement.
  std::vector<int32_t> labelledby_ids =
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_LABELLEDBY_IDS);
  ui::AXNameFrom nameFrom = static_cast<ui::AXNameFrom>(
      browserAccessibility_->GetIntAttribute(ui::AX_ATTR_NAME_FROM));
  if (nameFrom == ui::AX_NAME_FROM_RELATED_ELEMENT &&
      labelledby_ids.size() == 1 &&
      browserAccessibility_->manager()->GetFromID(labelledby_ids[0])) {
    return @"";
  }

  std::string name = browserAccessibility_->GetStringAttribute(
      ui::AX_ATTR_NAME);
  if (!name.empty()) {
    // On Mac OS X, the accessible name of an object is exposed as its
    // title if it comes from visible text, and as its description
    // otherwise, but never both.
    if (nameFrom == ui::AX_NAME_FROM_CONTENTS ||
        nameFrom == ui::AX_NAME_FROM_RELATED_ELEMENT ||
        nameFrom == ui::AX_NAME_FROM_VALUE) {
      return @"";
    } else {
      return base::SysUTF8ToNSString(name);
    }
  }

  // Given an image where there's no other title, return the base part
  // of the filename as the description.
  if ([[self role] isEqualToString:NSAccessibilityImageRole]) {
    if (browserAccessibility_->HasStringAttribute(ui::AX_ATTR_NAME))
      return @"";
    if ([self titleUIElement])
      return @"";

    std::string url;
    if (browserAccessibility_->GetStringAttribute(
            ui::AX_ATTR_URL, &url)) {
      // Given a url like http://foo.com/bar/baz.png, just return the
      // base name, e.g., "baz.png".
      size_t leftIndex = url.rfind('/');
      std::string basename =
          leftIndex != std::string::npos ? url.substr(leftIndex) : url;
      return base::SysUTF8ToNSString(basename);
    }
  }

  // If it's focusable but didn't have any other name or value, compute a name
  // from its descendants.
  base::string16 value = browserAccessibility_->GetValue();
  if (browserAccessibility_->HasState(ui::AX_STATE_FOCUSABLE) &&
      !browserAccessibility_->IsControl() &&
      value.empty() &&
      [self internalRole] != ui::AX_ROLE_DATE_TIME &&
      [self internalRole] != ui::AX_ROLE_WEB_AREA &&
      [self internalRole] != ui::AX_ROLE_ROOT_WEB_AREA) {
    return base::SysUTF8ToNSString(
        browserAccessibility_->ComputeAccessibleNameFromDescendants());
  }

  return @"";
}

- (NSNumber*)disclosing {
  if ([self internalRole] == ui::AX_ROLE_TREE_ITEM) {
    return [NSNumber numberWithBool:
        GetState(browserAccessibility_, ui::AX_STATE_EXPANDED)];
  } else {
    return nil;
  }
}

- (id)disclosedByRow {
  // The row that contains this row.
  // It should be the same as the first parent that is a treeitem.
  return nil;
}

- (NSNumber*)disclosureLevel {
  ui::AXRole role = [self internalRole];
  if (role == ui::AX_ROLE_ROW ||
      role == ui::AX_ROLE_TREE_ITEM) {
    int level = browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_HIERARCHICAL_LEVEL);
    // Mac disclosureLevel is 0-based, but web levels are 1-based.
    if (level > 0)
      level--;
    return [NSNumber numberWithInt:level];
  } else {
    return nil;
  }
}

- (id)disclosedRows {
  // The rows that are considered inside this row.
  return nil;
}

- (NSString*)dropeffect {
  std::string dropEffect;
  if (browserAccessibility_->GetHtmlAttribute("aria-dropeffect", &dropEffect))
    return base::SysUTF8ToNSString(dropEffect);

  return nil;
}

- (NSNumber*)enabled {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, ui::AX_STATE_ENABLED)];
}

- (NSNumber*)expanded {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, ui::AX_STATE_EXPANDED)];
}

- (NSNumber*)focused {
  BrowserAccessibilityManager* manager = browserAccessibility_->manager();
  NSNumber* ret = [NSNumber numberWithBool:
      manager->GetFocus(NULL) == browserAccessibility_];
  return ret;
}

- (NSNumber*)grabbed {
  std::string grabbed;
  if (browserAccessibility_->GetHtmlAttribute("aria-grabbed", &grabbed) &&
      grabbed == "true")
    return [NSNumber numberWithBool:YES];

  return [NSNumber numberWithBool:NO];
}

- (id)header {
  int headerElementId = -1;
  if ([self internalRole] == ui::AX_ROLE_TABLE ||
      [self internalRole] == ui::AX_ROLE_GRID) {
    browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_TABLE_HEADER_ID, &headerElementId);
  } else if ([self internalRole] == ui::AX_ROLE_COLUMN) {
    browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_TABLE_COLUMN_HEADER_ID, &headerElementId);
  } else if ([self internalRole] == ui::AX_ROLE_ROW) {
    browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_TABLE_ROW_HEADER_ID, &headerElementId);
  }

  if (headerElementId > 0) {
    BrowserAccessibility* headerObject =
        browserAccessibility_->manager()->GetFromID(headerElementId);
    if (headerObject)
      return headerObject->ToBrowserAccessibilityCocoa();
  }
  return nil;
}

- (NSString*)help {
  return NSStringForStringAttribute(
      browserAccessibility_, ui::AX_ATTR_DESCRIPTION);
}

- (NSNumber*)index {
  if ([self internalRole] == ui::AX_ROLE_COLUMN) {
    int columnIndex = browserAccessibility_->GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_INDEX);
    return [NSNumber numberWithInt:columnIndex];
  } else if ([self internalRole] == ui::AX_ROLE_ROW) {
    int rowIndex = browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_TABLE_ROW_INDEX);
    return [NSNumber numberWithInt:rowIndex];
  }

  return nil;
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  return [[self role] isEqualToString:NSAccessibilityUnknownRole];
}

- (NSString*)invalid {
  int invalidState;
  if (!browserAccessibility_->GetIntAttribute(
      ui::AX_ATTR_INVALID_STATE, &invalidState))
    return @"false";

  switch (invalidState) {
  case ui::AX_INVALID_STATE_FALSE:
    return @"false";
  case ui::AX_INVALID_STATE_TRUE:
    return @"true";
  case ui::AX_INVALID_STATE_SPELLING:
    return @"spelling";
  case ui::AX_INVALID_STATE_GRAMMAR:
    return @"grammar";
  case ui::AX_INVALID_STATE_OTHER:
    {
      std::string ariaInvalidValue;
      if (browserAccessibility_->GetStringAttribute(
          ui::AX_ATTR_ARIA_INVALID_VALUE,
          &ariaInvalidValue))
        return base::SysUTF8ToNSString(ariaInvalidValue);
      // Return @"true" since we cannot be more specific about the value.
      return @"true";
    }
  default:
    NOTREACHED();
  }

  return @"false";
}

- (NSString*)placeholderValue {
  ui::AXNameFrom nameFrom = static_cast<ui::AXNameFrom>(
      browserAccessibility_->GetIntAttribute(ui::AX_ATTR_NAME_FROM));
  if (nameFrom == ui::AX_NAME_FROM_PLACEHOLDER) {
    return NSStringForStringAttribute(
        browserAccessibility_, ui::AX_ATTR_NAME);
  }

  ui::AXDescriptionFrom descriptionFrom = static_cast<ui::AXDescriptionFrom>(
      browserAccessibility_->GetIntAttribute(ui::AX_ATTR_DESCRIPTION_FROM));
  if (descriptionFrom == ui::AX_DESCRIPTION_FROM_PLACEHOLDER) {
    return NSStringForStringAttribute(
        browserAccessibility_, ui::AX_ATTR_DESCRIPTION);
  }

  return NSStringForStringAttribute(
      browserAccessibility_, ui::AX_ATTR_PLACEHOLDER);
}

- (void)addLinkedUIElementsFromAttribute:(ui::AXIntListAttribute)attribute
                                   addTo:(NSMutableArray*)outArray {
  const std::vector<int32_t>& attributeValues =
      browserAccessibility_->GetIntListAttribute(attribute);
  for (size_t i = 0; i < attributeValues.size(); ++i) {
    BrowserAccessibility* element =
        browserAccessibility_->manager()->GetFromID(attributeValues[i]);
    if (element)
      [outArray addObject:element->ToBrowserAccessibilityCocoa()];
  }
}

- (NSArray*)linkedUIElements {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [self addLinkedUIElementsFromAttribute:ui::AX_ATTR_CONTROLS_IDS addTo:ret];
  [self addLinkedUIElementsFromAttribute:ui::AX_ATTR_FLOWTO_IDS addTo:ret];
  if ([ret count] == 0)
    return nil;
  return ret;
}

- (NSNumber*)loaded {
  return [NSNumber numberWithBool:YES];
}

- (NSNumber*)loadingProgress {
  BrowserAccessibilityManager* manager = browserAccessibility_->manager();
  float floatValue = manager->GetTreeData().loading_progress;
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)maxValue {
  float floatValue = browserAccessibility_->GetFloatAttribute(
      ui::AX_ATTR_MAX_VALUE_FOR_RANGE);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)minValue {
  float floatValue = browserAccessibility_->GetFloatAttribute(
      ui::AX_ATTR_MIN_VALUE_FOR_RANGE);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSString*)orientation {
  if (GetState(browserAccessibility_, ui::AX_STATE_VERTICAL))
    return NSAccessibilityVerticalOrientationValue;
  else if (GetState(browserAccessibility_, ui::AX_STATE_HORIZONTAL))
    return NSAccessibilityHorizontalOrientationValue;

  return @"";
}

- (NSNumber*)numberOfCharacters {
  base::string16 value = browserAccessibility_->GetValue();
  return [NSNumber numberWithInt:value.size()];
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
  if (browserAccessibility_->GetParent()) {
    return NSAccessibilityUnignoredAncestor(
        browserAccessibility_->GetParent()->ToBrowserAccessibilityCocoa());
  } else {
    // Hook back up to RenderWidgetHostViewCocoa.
    BrowserAccessibilityManagerMac* manager =
        static_cast<BrowserAccessibilityManagerMac*>(
            browserAccessibility_->manager());
    return manager->parent_view();
  }
}

- (NSValue*)position {
  NSPoint origin = [self origin];
  NSSize size = [[self size] sizeValue];
  NSPoint pointInScreen = [self pointInScreen:origin size:size];
  return [NSValue valueWithPoint:pointInScreen];
}

- (NSNumber*)required {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, ui::AX_STATE_REQUIRED)];
}

// Returns an enum indicating the role from browserAccessibility_.
- (ui::AXRole)internalRole {
  return static_cast<ui::AXRole>(browserAccessibility_->GetRole());
}

// Returns true if this should expose its accessible name in AXValue.
- (bool)shouldExposeNameInAXValue {
  switch ([self internalRole]) {
    case ui::AX_ROLE_LIST_BOX_OPTION:
    case ui::AX_ROLE_LIST_MARKER:
    case ui::AX_ROLE_MENU_LIST_OPTION:
    case ui::AX_ROLE_STATIC_TEXT:
      return true;
    default:
      return false;
  }
}

- (content::BrowserAccessibilityDelegate*)delegate {
  return browserAccessibility_->manager() ?
      browserAccessibility_->manager()->delegate() :
      nil;
}

- (content::BrowserAccessibility*)browserAccessibility {
  return browserAccessibility_;
}

- (NSPoint)pointInScreen:(NSPoint)origin
                    size:(NSSize)size {
  if (!browserAccessibility_)
    return NSZeroPoint;

  // Get the delegate for the topmost BrowserAccessibilityManager, because
  // that's the only one that can convert points to their origin in the screen.
  BrowserAccessibilityDelegate* delegate =
      browserAccessibility_->manager()->GetDelegateFromRootManager();
  if (delegate) {
    gfx::Rect bounds(origin.x, origin.y, size.width, size.height);
    gfx::Point point = delegate->AccessibilityOriginInScreen(bounds);
    return NSMakePoint(point.x(), point.y());
  } else {
    return NSZeroPoint;
  }
}

// Returns a string indicating the NSAccessibility role of this object.
- (NSString*)role {
  if (!browserAccessibility_)
    return nil;

  ui::AXRole role = [self internalRole];
  if (role == ui::AX_ROLE_CANVAS &&
      browserAccessibility_->GetBoolAttribute(
          ui::AX_ATTR_CANVAS_HAS_FALLBACK)) {
    return NSAccessibilityGroupRole;
  }
  if (role == ui::AX_ROLE_BUTTON || role == ui::AX_ROLE_TOGGLE_BUTTON) {
    bool isAriaPressedDefined;
    bool isMixed;
    browserAccessibility_->GetAriaTristate("aria-pressed",
                                           &isAriaPressedDefined,
                                           &isMixed);
    if (isAriaPressedDefined)
      return NSAccessibilityCheckBoxRole;
    else
      return NSAccessibilityButtonRole;
  }
  if ((browserAccessibility_->IsSimpleTextControl() &&
       browserAccessibility_->HasState(ui::AX_STATE_MULTILINE)) ||
      browserAccessibility_->IsRichTextControl()) {
    return NSAccessibilityTextAreaRole;
  }

  // If this is a web area for a presentational iframe, give it a role of
  // something other than WebArea so that the fact that it's a separate doc
  // is not exposed to AT.
  if (browserAccessibility_->IsWebAreaForPresentationalIframe())
    return NSAccessibilityGroupRole;

  return [AXPlatformNodeCocoa nativeRoleFromAXRole:role];
}

// Returns a string indicating the role description of this object.
- (NSString*)roleDescription {
  NSString* role = [self role];

  ContentClient* content_client = content::GetContentClient();

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

  if (([role isEqualToString:NSAccessibilityGroupRole] ||
       [role isEqualToString:NSAccessibilityRadioButtonRole]) &&
      !browserAccessibility_->IsWebAreaForPresentationalIframe()) {
    std::string role;
    if (browserAccessibility_->GetHtmlAttribute("role", &role)) {
      ui::AXRole internalRole = [self internalRole];
      if ((internalRole != ui::AX_ROLE_GROUP &&
           internalRole != ui::AX_ROLE_LIST_ITEM) ||
          internalRole == ui::AX_ROLE_TAB) {
        // TODO(dtseng): This is not localized; see crbug/84814.
        return base::SysUTF8ToNSString(role);
      }
    }
  }

  switch([self internalRole]) {
  case ui::AX_ROLE_ARTICLE:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_ARTICLE));
  case ui::AX_ROLE_BANNER:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_BANNER));
  case ui::AX_ROLE_CHECK_BOX:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_CHECK_BOX));
  case ui::AX_ROLE_COMPLEMENTARY:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_COMPLEMENTARY));
  case ui::AX_ROLE_CONTENT_INFO:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_ADDRESS));
  case ui::AX_ROLE_DESCRIPTION_LIST:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_DESCRIPTION_LIST));
  case ui::AX_ROLE_DESCRIPTION_LIST_DETAIL:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_DESCRIPTION_DETAIL));
  case ui::AX_ROLE_DESCRIPTION_LIST_TERM:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_DESCRIPTION_TERM));
  case ui::AX_ROLE_FIGURE:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_FIGURE));
  case ui::AX_ROLE_FOOTER:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_FOOTER));
  case ui::AX_ROLE_FORM:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_FORM));
  case ui::AX_ROLE_MAIN:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_MAIN_CONTENT));
  case ui::AX_ROLE_MARK:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_MARK));
  case ui::AX_ROLE_MATH:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_MATH));
  case ui::AX_ROLE_NAVIGATION:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_NAVIGATIONAL_LINK));
  case ui::AX_ROLE_REGION:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_REGION));
  case ui::AX_ROLE_SPIN_BUTTON:
    // This control is similar to what VoiceOver calls a "stepper".
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_STEPPER));
  case ui::AX_ROLE_STATUS:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_STATUS));
  case ui::AX_ROLE_SEARCH_BOX:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_SEARCH_BOX));
  case ui::AX_ROLE_SWITCH:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_SWITCH));
  case ui::AX_ROLE_TOGGLE_BUTTON:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_TOGGLE_BUTTON));
  default:
    break;
  }

  return NSAccessibilityRoleDescription(role, nil);
}

- (NSArray*)rowHeaders {
  if ([self internalRole] != ui::AX_ROLE_TABLE &&
      [self internalRole] != ui::AX_ROLE_GRID) {
    return nil;
  }

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  const std::vector<int32_t>& uniqueCellIds =
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_UNIQUE_CELL_IDS);
  for (size_t i = 0; i < uniqueCellIds.size(); ++i) {
    int id = uniqueCellIds[i];
    BrowserAccessibility* cell =
        browserAccessibility_->manager()->GetFromID(id);
    if (cell && cell->GetRole() == ui::AX_ROLE_ROW_HEADER)
      [ret addObject:cell->ToBrowserAccessibilityCocoa()];
  }
  return ret;
}

- (NSValue*)rowIndexRange {
  if (!browserAccessibility_->IsCellOrTableHeaderRole())
    return nil;

  int row = -1;
  int rowspan = -1;
  browserAccessibility_->GetIntAttribute(
      ui::AX_ATTR_TABLE_CELL_ROW_INDEX, &row);
  browserAccessibility_->GetIntAttribute(
      ui::AX_ATTR_TABLE_CELL_ROW_SPAN, &rowspan);
  if (row >= 0 && rowspan >= 1)
    return [NSValue valueWithRange:NSMakeRange(row, rowspan)];
  return nil;
}

- (NSArray*)rows {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  if ([self internalRole] == ui::AX_ROLE_TABLE||
      [self internalRole] == ui::AX_ROLE_GRID) {
    for (BrowserAccessibilityCocoa* child in [self children]) {
      if ([[child role] isEqualToString:NSAccessibilityRowRole])
        [ret addObject:child];
    }
  } else if ([self internalRole] == ui::AX_ROLE_COLUMN) {
    const std::vector<int32_t>& indirectChildIds =
        browserAccessibility_->GetIntListAttribute(
            ui::AX_ATTR_INDIRECT_CHILD_IDS);
    for (uint32_t i = 0; i < indirectChildIds.size(); ++i) {
      int id = indirectChildIds[i];
      BrowserAccessibility* rowElement =
          browserAccessibility_->manager()->GetFromID(id);
      if (rowElement)
        [ret addObject:rowElement->ToBrowserAccessibilityCocoa()];
    }
  }

  return ret;
}

- (NSArray*)selectedChildren {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  BrowserAccessibilityManager* manager = browserAccessibility_->manager();
  BrowserAccessibility* focusedChild = manager->GetFocus(browserAccessibility_);

  // If it's not multiselectable, try to skip iterating over the
  // children.
  if (!GetState(browserAccessibility_, ui::AX_STATE_MULTISELECTABLE)) {
    // First try the focused child.
    if (focusedChild && focusedChild != browserAccessibility_) {
      [ret addObject:focusedChild->ToBrowserAccessibilityCocoa()];
      return ret;
    }

    // Next try the active descendant.
    int activeDescendantId;
    if (browserAccessibility_->GetIntAttribute(
            ui::AX_ATTR_ACTIVEDESCENDANT_ID, &activeDescendantId)) {
      BrowserAccessibility* activeDescendant =
          manager->GetFromID(activeDescendantId);
      if (activeDescendant) {
        [ret addObject:activeDescendant->ToBrowserAccessibilityCocoa()];
        return ret;
      }
    }
  }

  // If it's multiselectable or if the previous attempts failed,
  // return any children with the "selected" state, which may
  // come from aria-selected.
  uint32_t childCount = browserAccessibility_->PlatformChildCount();
  for (uint32_t index = 0; index < childCount; ++index) {
    BrowserAccessibility* child =
      browserAccessibility_->PlatformGetChild(index);
    if (child->HasState(ui::AX_STATE_SELECTED))
      [ret addObject:child->ToBrowserAccessibilityCocoa()];
  }

  // And if nothing's selected but one has focus, use the focused one.
  if ([ret count] == 0 &&
      focusedChild &&
      focusedChild != browserAccessibility_) {
    [ret addObject:focusedChild->ToBrowserAccessibilityCocoa()];
  }

  return ret;
}

// Returns the size of this object.
- (NSValue*)size {
  gfx::Rect bounds = browserAccessibility_->GetLocalBoundsRect();
  return  [NSValue valueWithSize:NSMakeSize(bounds.width(), bounds.height())];
}

- (NSString*)sortDirection {
  int sortDirection;
  if (!browserAccessibility_->GetIntAttribute(
      ui::AX_ATTR_SORT_DIRECTION, &sortDirection))
    return @"";

  switch (sortDirection) {
  case ui::AX_SORT_DIRECTION_UNSORTED:
    return @"";
  case ui::AX_SORT_DIRECTION_ASCENDING:
    return @"AXSortDirectionAscending";
  case ui::AX_SORT_DIRECTION_DESCENDING:
    return @"AXSortDirectionDescending";
  case ui::AX_SORT_DIRECTION_OTHER:
    return @"AXSortDirectionUnknown";
  default:
    NOTREACHED();
  }

  return @"";
}

// Returns a subrole based upon the role.
- (NSString*) subrole {
  ui::AXRole browserAccessibilityRole = [self internalRole];
  if (browserAccessibilityRole == ui::AX_ROLE_TEXT_FIELD &&
      GetState(browserAccessibility_, ui::AX_STATE_PROTECTED)) {
    return @"AXSecureTextField";
  }

  if (browserAccessibilityRole == ui::AX_ROLE_DESCRIPTION_LIST)
    return @"AXDefinitionList";

  if (browserAccessibilityRole == ui::AX_ROLE_LIST)
    return @"AXContentList";

  return [AXPlatformNodeCocoa nativeSubroleFromAXRole:browserAccessibilityRole];
}

// Returns all tabs in this subtree.
- (NSArray*)tabs {
  NSMutableArray* tabSubtree = [[[NSMutableArray alloc] init] autorelease];

  if ([self internalRole] == ui::AX_ROLE_TAB)
    [tabSubtree addObject:self];

  for (uint i=0; i < [[self children] count]; ++i) {
    NSArray* tabChildren = [[[self children] objectAtIndex:i] tabs];
    if ([tabChildren count] > 0)
      [tabSubtree addObjectsFromArray:tabChildren];
  }

  return tabSubtree;
}

- (NSString*)title {
  // Mac OS X wants static text exposed in AXValue.
  if ([self shouldExposeNameInAXValue])
    return @"";

  // If the name came from a single related element and it's present in the
  // tree, it will be exposed in AXTitleUIElement.
  std::vector<int32_t> labelledby_ids =
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_LABELLEDBY_IDS);
  ui::AXNameFrom nameFrom = static_cast<ui::AXNameFrom>(
      browserAccessibility_->GetIntAttribute(ui::AX_ATTR_NAME_FROM));
  if (nameFrom == ui::AX_NAME_FROM_RELATED_ELEMENT &&
      labelledby_ids.size() == 1 &&
      browserAccessibility_->manager()->GetFromID(labelledby_ids[0])) {
    return @"";
  }

  // On Mac OS X, the accessible name of an object is exposed as its
  // title if it comes from visible text, and as its description
  // otherwise, but never both.
  if (nameFrom == ui::AX_NAME_FROM_CONTENTS ||
      nameFrom == ui::AX_NAME_FROM_RELATED_ELEMENT ||
      nameFrom == ui::AX_NAME_FROM_VALUE) {
    return NSStringForStringAttribute(
        browserAccessibility_, ui::AX_ATTR_NAME);
  }

  return nil;
}

- (id)titleUIElement {
  std::vector<int32_t> labelledby_ids =
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_LABELLEDBY_IDS);
  ui::AXNameFrom nameFrom = static_cast<ui::AXNameFrom>(
      browserAccessibility_->GetIntAttribute(ui::AX_ATTR_NAME_FROM));
  if (nameFrom == ui::AX_NAME_FROM_RELATED_ELEMENT &&
      labelledby_ids.size() == 1) {
    BrowserAccessibility* titleElement =
        browserAccessibility_->manager()->GetFromID(labelledby_ids[0]);
    if (titleElement)
      return titleElement->ToBrowserAccessibilityCocoa();
  }

  return nil;
}

- (NSURL*)url {
  std::string url;
  if ([[self role] isEqualToString:@"AXWebArea"])
    url = browserAccessibility_->manager()->GetTreeData().url;
  else
    url = browserAccessibility_->GetStringAttribute(ui::AX_ATTR_URL);

  if (url.empty())
    return nil;

  return [NSURL URLWithString:(base::SysUTF8ToNSString(url))];
}

- (id)value {
  NSString* role = [self role];
  if ([self shouldExposeNameInAXValue]) {
    return NSStringForStringAttribute(
        browserAccessibility_, ui::AX_ATTR_NAME);
  } else if ([role isEqualToString:@"AXHeading"]) {
    int level = 0;
    if (browserAccessibility_->GetIntAttribute(
            ui::AX_ATTR_HIERARCHICAL_LEVEL, &level)) {
      return [NSNumber numberWithInt:level];
    }
  } else if ([role isEqualToString:NSAccessibilityButtonRole]) {
    // AXValue does not make sense for pure buttons.
    return @"";
  } else if ([self internalRole] == ui::AX_ROLE_TOGGLE_BUTTON) {
    int value = 0;
    bool isAriaPressedDefined;
    bool isMixed;
    value = browserAccessibility_->GetAriaTristate(
        "aria-pressed", &isAriaPressedDefined, &isMixed) ? 1 : 0;

    if (isMixed)
      value = 2;

    return [NSNumber numberWithInt:value];

  } else if ([role isEqualToString:NSAccessibilityCheckBoxRole] ||
             [role isEqualToString:NSAccessibilityRadioButtonRole]) {
    int value = 0;
    value = GetState(
        browserAccessibility_, ui::AX_STATE_CHECKED) ? 1 : 0;
    value = GetState(
        browserAccessibility_, ui::AX_STATE_SELECTED) ?
            1 :
            value;

    if (browserAccessibility_->GetBoolAttribute(ui::AX_ATTR_STATE_MIXED)) {
      value = 2;
    }
    return [NSNumber numberWithInt:value];
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityIncrementorRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole]) {
    float floatValue;
    if (browserAccessibility_->GetFloatAttribute(
            ui::AX_ATTR_VALUE_FOR_RANGE, &floatValue)) {
      return [NSNumber numberWithFloat:floatValue];
    }
  } else if ([role isEqualToString:NSAccessibilityColorWellRole]) {
    int color = browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_COLOR_VALUE);
    int red = (color >> 16) & 0xFF;
    int green = (color >> 8) & 0xFF;
    int blue = color & 0xFF;
    // This string matches the one returned by a native Mac color well.
    return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1",
                red / 255., green / 255., blue / 255.];
  }

  return base::SysUTF16ToNSString(browserAccessibility_->GetValue());
}

- (NSString*)valueDescription {
  if (browserAccessibility_)
    return base::SysUTF16ToNSString(browserAccessibility_->GetValue());
  return nil;
}

- (NSValue*)visibleCharacterRange {
  base::string16 value = browserAccessibility_->GetValue();
  return [NSValue valueWithRange:NSMakeRange(0, value.size())];
}

- (NSArray*)visibleCells {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  const std::vector<int32_t>& uniqueCellIds =
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_UNIQUE_CELL_IDS);
  for (size_t i = 0; i < uniqueCellIds.size(); ++i) {
    int id = uniqueCellIds[i];
    BrowserAccessibility* cell =
        browserAccessibility_->manager()->GetFromID(id);
    if (cell)
      [ret addObject:cell->ToBrowserAccessibilityCocoa()];
  }
  return ret;
}

- (NSArray*)visibleChildren {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  uint32_t childCount = browserAccessibility_->PlatformChildCount();
  for (uint32_t index = 0; index < childCount; ++index) {
    BrowserAccessibilityCocoa* child =
        browserAccessibility_->PlatformGetChild(index)->
            ToBrowserAccessibilityCocoa();
    [ret addObject:child];
  }
  return ret;
}

- (NSArray*)visibleColumns {
  return [self columns];
}

- (NSArray*)visibleRows {
  return [self rows];
}

- (NSNumber*)visited {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, ui::AX_STATE_VISITED)];
}

- (id)window {
  if (!browserAccessibility_)
    return nil;

  BrowserAccessibilityManagerMac* manager =
      static_cast<BrowserAccessibilityManagerMac*>(
          browserAccessibility_->manager());
  if (!manager || !manager->parent_view())
    return nil;

  return [manager->parent_view() window];
}

- (NSString*)methodNameForAttribute:(NSString*)attribute {
  return [attributeToMethodNameMap objectForKey:attribute];
}

- (void)swapChildren:(base::scoped_nsobject<NSMutableArray>*)other {
  children_.swap(*other);
}

// Returns the requested text range from this object's value attribute.
- (NSString*)valueForRange:(NSRange)range {
  if (!browserAccessibility_)
    return nil;

  base::string16 value = browserAccessibility_->GetValue();
  if (NSMaxRange(range) > value.size())
    return nil;

  return base::SysUTF16ToNSString(value.substr(range.location, range.length));
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (!browserAccessibility_)
    return nil;

  SEL selector =
      NSSelectorFromString([self methodNameForAttribute:attribute]);
  if (selector)
    return [self performSelector:selector];

  // TODO(dtseng): refactor remaining attributes.
  int selStart, selEnd;
  if (browserAccessibility_->GetIntAttribute(
          ui::AX_ATTR_TEXT_SEL_START, &selStart) &&
      browserAccessibility_->
          GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END, &selEnd)) {
    if (selStart > selEnd)
      std::swap(selStart, selEnd);
    int selLength = selEnd - selStart;
    if ([attribute isEqualToString:
        NSAccessibilityInsertionPointLineNumberAttribute]) {
      const std::vector<int32_t>& line_breaks =
          browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_LINE_BREAKS);
      for (int i = 0; i < static_cast<int>(line_breaks.size()); ++i) {
        if (line_breaks[i] > selStart)
          return [NSNumber numberWithInt:i];
      }
      return [NSNumber numberWithInt:static_cast<int>(line_breaks.size())];
    }
    if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
      base::string16 value = browserAccessibility_->GetValue();
      return base::SysUTF16ToNSString(value.substr(selStart, selLength));
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
  if (!browserAccessibility_)
    return nil;

  const std::vector<int32_t>& line_breaks =
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_LINE_BREAKS);
  base::string16 value = browserAccessibility_->GetValue();
  int len = static_cast<int>(value.size());

  if ([attribute isEqualToString:
      NSAccessibilityStringForRangeParameterizedAttribute]) {
    return [self valueForRange:[(NSValue*)parameter rangeValue]];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityAttributedStringForRangeParameterizedAttribute]) {
    NSString* value = [self valueForRange:[(NSValue*)parameter rangeValue]];
    return [[[NSAttributedString alloc] initWithString:value] autorelease];
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

  if ([attribute isEqualToString:
      NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
    if ([self internalRole] != ui::AX_ROLE_TABLE &&
        [self internalRole] != ui::AX_ROLE_GRID) {
      return nil;
    }
    if (![parameter isKindOfClass:[NSArray self]])
      return nil;
    NSArray* array = parameter;
    int column = [[array objectAtIndex:0] intValue];
    int row = [[array objectAtIndex:1] intValue];
    int num_columns = browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_TABLE_COLUMN_COUNT);
    int num_rows = browserAccessibility_->GetIntAttribute(
        ui::AX_ATTR_TABLE_ROW_COUNT);
    if (column < 0 || column >= num_columns ||
        row < 0 || row >= num_rows) {
      return nil;
    }
    for (size_t i = 0;
         i < browserAccessibility_->PlatformChildCount();
         ++i) {
      BrowserAccessibility* child = browserAccessibility_->PlatformGetChild(i);
      if (child->GetRole() != ui::AX_ROLE_ROW)
        continue;
      int rowIndex;
      if (!child->GetIntAttribute(
              ui::AX_ATTR_TABLE_ROW_INDEX, &rowIndex)) {
        continue;
      }
      if (rowIndex < row)
        continue;
      if (rowIndex > row)
        break;
      for (size_t j = 0;
           j < child->PlatformChildCount();
           ++j) {
        BrowserAccessibility* cell = child->PlatformGetChild(j);
        if (!cell->IsCellOrTableHeaderRole())
          continue;
        int colIndex;
        if (!cell->GetIntAttribute(
                ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX,
                &colIndex)) {
          continue;
        }
        if (colIndex == column)
          return cell->ToBrowserAccessibilityCocoa();
        if (colIndex > column)
          break;
      }
    }
    return nil;
  }

  if ([attribute isEqualToString:
      NSAccessibilityBoundsForRangeParameterizedAttribute]) {
    if ([self internalRole] != ui::AX_ROLE_STATIC_TEXT)
      return nil;
    NSRange range = [(NSValue*)parameter rangeValue];
    gfx::Rect rect = browserAccessibility_->GetGlobalBoundsForRange(
        range.location, range.length);
    NSPoint origin = NSMakePoint(rect.x(), rect.y());
    NSSize size = NSMakeSize(rect.width(), rect.height());
    NSPoint pointInScreen = [self pointInScreen:origin size:size];
    NSRect nsrect = NSMakeRect(
        pointInScreen.x, pointInScreen.y, rect.width(), rect.height());
    return [NSValue valueWithRect:nsrect];
  }
  if ([attribute isEqualToString:@"AXUIElementCountForSearchPredicate"]) {
    OneShotAccessibilityTreeSearch search(browserAccessibility_);
    if (InitializeAccessibilityTreeSearch(&search, parameter))
      return [NSNumber numberWithInt:search.CountMatches()];
    return nil;
  }

  if ([attribute isEqualToString:@"AXUIElementsForSearchPredicate"]) {
    OneShotAccessibilityTreeSearch search(browserAccessibility_);
    if (InitializeAccessibilityTreeSearch(&search, parameter)) {
      size_t count = search.CountMatches();
      NSMutableArray* result = [NSMutableArray arrayWithCapacity:count];
      for (size_t i = 0; i < count; ++i) {
        BrowserAccessibility* match = search.GetMatchAtIndex(i);
        [result addObject:match->ToBrowserAccessibilityCocoa()];
      }
      return result;
    }
    return nil;
  }

  // TODO(dtseng): support the following attributes.
  if ([attribute isEqualTo:
          NSAccessibilityRangeForPositionParameterizedAttribute] ||
      [attribute isEqualTo:
          NSAccessibilityRangeForIndexParameterizedAttribute] ||
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
  if (!browserAccessibility_)
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray arrayWithObjects:
      @"AXUIElementCountForSearchPredicate",
      @"AXUIElementsForSearchPredicate",
      nil];

  if ([[self role] isEqualToString:NSAccessibilityTableRole] ||
      [[self role] isEqualToString:NSAccessibilityGridRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityCellForColumnAndRowParameterizedAttribute,
        nil]];
  }

  if (browserAccessibility_->HasState(ui::AX_STATE_EDITABLE)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityLineForIndexParameterizedAttribute,
        NSAccessibilityRangeForLineParameterizedAttribute,
        NSAccessibilityStringForRangeParameterizedAttribute,
        NSAccessibilityRangeForPositionParameterizedAttribute,
        NSAccessibilityRangeForIndexParameterizedAttribute,
        NSAccessibilityBoundsForRangeParameterizedAttribute,
        NSAccessibilityRTFForRangeParameterizedAttribute,
        NSAccessibilityAttributedStringForRangeParameterizedAttribute,
        NSAccessibilityStyleRangeForIndexParameterizedAttribute,
        nil]];
  }

  if ([self internalRole] == ui::AX_ROLE_STATIC_TEXT) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityBoundsForRangeParameterizedAttribute,
        nil]];
  }

  return ret;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  if (!browserAccessibility_)
    return nil;

  NSMutableArray* ret =
      [NSMutableArray arrayWithObject:NSAccessibilityShowMenuAction];
  NSString* role = [self role];
  // TODO(dtseng): this should only get set when there's a default action.
  if (![role isEqualToString:NSAccessibilityStaticTextRole] &&
      ![role isEqualToString:NSAccessibilityTextFieldRole] &&
      ![role isEqualToString:NSAccessibilityTextAreaRole]) {
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
  if (!browserAccessibility_)
    return nil;

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
  if (!browserAccessibility_)
    return 0;

  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

// Returns the list of accessibility attributes that this object supports.
- (NSArray*)accessibilityAttributeNames {
  if (!browserAccessibility_)
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray arrayWithObjects:
      NSAccessibilityChildrenAttribute,
      NSAccessibilityDescriptionAttribute,
      NSAccessibilityEnabledAttribute,
      NSAccessibilityFocusedAttribute,
      NSAccessibilityHelpAttribute,
      NSAccessibilityLinkedUIElementsAttribute,
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
      @"AXAccessKey",
      @"AXInvalid",
      @"AXVisited",
      nil];

  // Specific role attributes.
  NSString* role = [self role];
  NSString* subrole = [self subrole];
  if ([role isEqualToString:NSAccessibilityTableRole] ||
      [role isEqualToString:NSAccessibilityGridRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityColumnsAttribute,
        NSAccessibilityVisibleColumnsAttribute,
        NSAccessibilityRowsAttribute,
        NSAccessibilityVisibleRowsAttribute,
        NSAccessibilityVisibleCellsAttribute,
        NSAccessibilityHeaderAttribute,
        NSAccessibilityColumnHeaderUIElementsAttribute,
        NSAccessibilityRowHeaderUIElementsAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityColumnRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityIndexAttribute,
        NSAccessibilityHeaderAttribute,
        NSAccessibilityRowsAttribute,
        NSAccessibilityVisibleRowsAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityCellRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityColumnIndexRangeAttribute,
        NSAccessibilityRowIndexRangeAttribute,
        @"AXSortDirection",
        nil]];
  } else if ([role isEqualToString:@"AXWebArea"]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXLoaded",
        @"AXLoadingProgress",
        nil]];
  } else if ([role isEqualToString:NSAccessibilityTabGroupRole]) {
    [ret addObject:NSAccessibilityTabsAttribute];
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityIncrementorRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityMaxValueAttribute,
        NSAccessibilityMinValueAttribute,
        NSAccessibilityValueDescriptionAttribute,
        nil]];
  } else if ([subrole isEqualToString:NSAccessibilityOutlineRowSubrole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityDisclosingAttribute,
        NSAccessibilityDisclosedByRowAttribute,
        NSAccessibilityDisclosureLevelAttribute,
        NSAccessibilityDisclosedRowsAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityRowRole]) {
    if (browserAccessibility_->GetParent()) {
      base::string16 parentRole;
      browserAccessibility_->GetParent()->GetHtmlAttribute(
          "role", &parentRole);
      const base::string16 treegridRole(base::ASCIIToUTF16("treegrid"));
      if (parentRole == treegridRole) {
        [ret addObjectsFromArray:[NSArray arrayWithObjects:
            NSAccessibilityDisclosingAttribute,
            NSAccessibilityDisclosedByRowAttribute,
            NSAccessibilityDisclosureLevelAttribute,
            NSAccessibilityDisclosedRowsAttribute,
            nil]];
      } else {
        [ret addObjectsFromArray:[NSArray arrayWithObjects:
            NSAccessibilityIndexAttribute,
            nil]];
      }
    }
  } else if ([role isEqualToString:NSAccessibilityListRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilitySelectedChildrenAttribute,
        NSAccessibilityVisibleChildrenAttribute,
        nil]];
  }

  // Caret navigation and text selection attributes.
  if (browserAccessibility_->HasState(ui::AX_STATE_EDITABLE)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityInsertionPointLineNumberAttribute,
        NSAccessibilityNumberOfCharactersAttribute,
        NSAccessibilitySelectedTextAttribute,
        NSAccessibilitySelectedTextRangeAttribute,
        NSAccessibilityVisibleCharacterRangeAttribute,
        nil]];
  }

  // Add the url attribute only if it has a valid url.
  if ([self url] != nil) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityURLAttribute,
        nil]];
  }

  // Position in set and Set size
  if (browserAccessibility_->HasIntAttribute(ui::AX_ATTR_POS_IN_SET)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
         @"AXARIAPosInSet",
         nil]];
  }
  if (browserAccessibility_->HasIntAttribute(ui::AX_ATTR_SET_SIZE)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
         @"AXARIASetSize",
         nil]];
  }

  // Live regions.
  if (browserAccessibility_->HasStringAttribute(
          ui::AX_ATTR_LIVE_STATUS)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIALive",
        nil]];
  }
  if (browserAccessibility_->HasStringAttribute(
          ui::AX_ATTR_LIVE_RELEVANT)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIARelevant",
        nil]];
  }
  if (browserAccessibility_->HasBoolAttribute(
          ui::AX_ATTR_LIVE_ATOMIC)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIAAtomic",
        nil]];
  }
  if (browserAccessibility_->HasBoolAttribute(
          ui::AX_ATTR_LIVE_BUSY)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIABusy",
        nil]];
  }

  std::string dropEffect;
  if (browserAccessibility_->GetHtmlAttribute("aria-dropeffect", &dropEffect)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXDropEffects",
        nil]];
  }

  std::string grabbed;
  if (browserAccessibility_->GetHtmlAttribute("aria-grabbed", &grabbed)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXGrabbed",
        nil]];
  }

  // Add expanded attribute only if it has expanded or collapsed state.
  if (GetState(browserAccessibility_, ui::AX_STATE_EXPANDED) ||
        GetState(browserAccessibility_, ui::AX_STATE_COLLAPSED)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityExpandedAttribute,
        nil]];
  }

  if (GetState(browserAccessibility_, ui::AX_STATE_VERTICAL)
      || GetState(browserAccessibility_, ui::AX_STATE_HORIZONTAL)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityOrientationAttribute, nil]];
  }

  if (browserAccessibility_->HasStringAttribute(ui::AX_ATTR_PLACEHOLDER)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityPlaceholderValueAttribute, nil]];
  }

  if (GetState(browserAccessibility_, ui::AX_STATE_REQUIRED)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXRequired", nil]];
  }

  // Title UI Element.
  if (browserAccessibility_->HasIntListAttribute(ui::AX_ATTR_LABELLEDBY_IDS) &&
      browserAccessibility_->GetIntListAttribute(ui::AX_ATTR_LABELLEDBY_IDS)
                            .size() > 0) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
         NSAccessibilityTitleUIElementAttribute,
         nil]];
  }
  // TODO(aboxhall): expose NSAccessibilityServesAsTitleForUIElementsAttribute
  // for elements which are referred to by labelledby or are labels

  return ret;
}

// Returns the index of the child in this objects array of children.
- (NSUInteger)accessibilityGetIndexOf:(id)child {
  if (!browserAccessibility_)
    return 0;

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
  if (!browserAccessibility_)
    return NO;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    if ([self internalRole] == ui::AX_ROLE_DATE_TIME)
      return NO;

    return GetState(browserAccessibility_, ui::AX_STATE_FOCUSABLE);
  }

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    return browserAccessibility_->GetBoolAttribute(
        ui::AX_ATTR_CAN_SET_VALUE);
  }

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] &&
      browserAccessibility_->HasState(ui::AX_STATE_EDITABLE)) {
    return YES;
  }

  return NO;
}

// Returns whether or not this object should be ignored in the accessibility
// tree.
- (BOOL)accessibilityIsIgnored {
  if (!browserAccessibility_)
    return YES;

  return [self isIgnored];
}

// Performs the given accessibility action on the webkit accessibility object
// that backs this object.
- (void)accessibilityPerformAction:(NSString*)action {
  if (!browserAccessibility_)
    return;

  // TODO(dmazzoni): Support more actions.
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    [self delegate]->AccessibilityDoDefaultAction(
        browserAccessibility_->GetId());
  } else if ([action isEqualToString:NSAccessibilityShowMenuAction]) {
    [self delegate]->AccessibilityShowContextMenu(
        browserAccessibility_->GetId());
  }
}

// Returns the description of the given action.
- (NSString*)accessibilityActionDescription:(NSString*)action {
  if (!browserAccessibility_)
    return nil;

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
  if (!browserAccessibility_)
    return;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    BrowserAccessibilityManager* manager = browserAccessibility_->manager();
    NSNumber* focusedNumber = value;
    BOOL focused = [focusedNumber intValue];
    if (focused)
      manager->SetFocus(browserAccessibility_, true);
  }
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    NSRange range = [(NSValue*)value rangeValue];
    [self delegate]->AccessibilitySetSelection(
        browserAccessibility_->GetId(), range.location,
        browserAccessibility_->GetId(), range.location + range.length);
  }
}

// Returns the deepest accessibility child that should not be ignored.
// It is assumed that the hit test has been narrowed down to this object
// or one of its children, so this will never return nil unless this
// object is invalid.
- (id)accessibilityHitTest:(NSPoint)point {
  if (!browserAccessibility_)
    return nil;

  BrowserAccessibilityCocoa* hit = self;
  for (BrowserAccessibilityCocoa* child in [self children]) {
    if (!child->browserAccessibility_)
      continue;
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
  return browserAccessibility_->GetId();
}

- (BOOL)accessibilityShouldUseUniqueId {
  return YES;
}

@end
