// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility_node_serializer.h"

#include <set>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityRole.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocumentType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

using content::AccessibilityNodeData;
using WebKit::WebAccessibilityRole;
using WebKit::WebAccessibilityObject;
using WebKit::WebDocument;
using WebKit::WebDocumentType;
using WebKit::WebElement;
using WebKit::WebNode;
using WebKit::WebVector;

namespace {

// Returns true if |ancestor| is the first unignored parent of |child|,
// which means that when walking up the parent chain from |child|,
// |ancestor| is the *first* ancestor that isn't marked as
// accessibilityIsIgnored().
bool IsParentUnignoredOf(const WebAccessibilityObject& ancestor,
                         const WebAccessibilityObject& child) {
  WebAccessibilityObject parent = child.parentObject();
  while (!parent.isDetached() && parent.accessibilityIsIgnored())
    parent = parent.parentObject();
  return parent.equals(ancestor);
}

// Provides a conversion between the WebKit::WebAccessibilityRole and a role
// supported on the Browser side. Listed alphabetically by the
// WebKit::WebAccessibilityRole (except for default role).
AccessibilityNodeData::Role ConvertRole(WebKit::WebAccessibilityRole role) {
  switch (role) {
    case WebKit::WebAccessibilityRoleAnnotation:
      return AccessibilityNodeData::ROLE_ANNOTATION;
    case WebKit::WebAccessibilityRoleApplication:
      return AccessibilityNodeData::ROLE_APPLICATION;
    case WebKit::WebAccessibilityRoleApplicationAlert:
      return AccessibilityNodeData::ROLE_ALERT;
    case WebKit::WebAccessibilityRoleApplicationAlertDialog:
      return AccessibilityNodeData::ROLE_ALERT_DIALOG;
    case WebKit::WebAccessibilityRoleApplicationDialog:
      return AccessibilityNodeData::ROLE_DIALOG;
    case WebKit::WebAccessibilityRoleApplicationLog:
      return AccessibilityNodeData::ROLE_LOG;
    case WebKit::WebAccessibilityRoleApplicationMarquee:
      return AccessibilityNodeData::ROLE_MARQUEE;
    case WebKit::WebAccessibilityRoleApplicationStatus:
      return AccessibilityNodeData::ROLE_STATUS;
    case WebKit::WebAccessibilityRoleApplicationTimer:
      return AccessibilityNodeData::ROLE_TIMER;
    case WebKit::WebAccessibilityRoleBrowser:
      return AccessibilityNodeData::ROLE_BROWSER;
    case WebKit::WebAccessibilityRoleBusyIndicator:
      return AccessibilityNodeData::ROLE_BUSY_INDICATOR;
    case WebKit::WebAccessibilityRoleButton:
      return AccessibilityNodeData::ROLE_BUTTON;
    case WebKit::WebAccessibilityRoleCanvas:
      return AccessibilityNodeData::ROLE_CANVAS;
    case WebKit::WebAccessibilityRoleCell:
      return AccessibilityNodeData::ROLE_CELL;
    case WebKit::WebAccessibilityRoleCheckBox:
      return AccessibilityNodeData::ROLE_CHECKBOX;
    case WebKit::WebAccessibilityRoleColorWell:
      return AccessibilityNodeData::ROLE_COLOR_WELL;
    case WebKit::WebAccessibilityRoleColumn:
      return AccessibilityNodeData::ROLE_COLUMN;
    case WebKit::WebAccessibilityRoleColumnHeader:
      return AccessibilityNodeData::ROLE_COLUMN_HEADER;
    case WebKit::WebAccessibilityRoleComboBox:
      return AccessibilityNodeData::ROLE_COMBO_BOX;
    case WebKit::WebAccessibilityRoleDefinitionListDefinition:
      return AccessibilityNodeData::ROLE_DEFINITION_LIST_DEFINITION;
    case WebKit::WebAccessibilityRoleDefinitionListTerm:
      return AccessibilityNodeData::ROLE_DEFINITION_LIST_TERM;
    case WebKit::WebAccessibilityRoleDirectory:
      return AccessibilityNodeData::ROLE_DIRECTORY;
    case WebKit::WebAccessibilityRoleDisclosureTriangle:
      return AccessibilityNodeData::ROLE_DISCLOSURE_TRIANGLE;
    case WebKit::WebAccessibilityRoleDiv:
      return AccessibilityNodeData::ROLE_DIV;
    case WebKit::WebAccessibilityRoleDocument:
      return AccessibilityNodeData::ROLE_DOCUMENT;
    case WebKit::WebAccessibilityRoleDocumentArticle:
      return AccessibilityNodeData::ROLE_ARTICLE;
    case WebKit::WebAccessibilityRoleDocumentMath:
      return AccessibilityNodeData::ROLE_MATH;
    case WebKit::WebAccessibilityRoleDocumentNote:
      return AccessibilityNodeData::ROLE_NOTE;
    case WebKit::WebAccessibilityRoleDocumentRegion:
      return AccessibilityNodeData::ROLE_REGION;
    case WebKit::WebAccessibilityRoleDrawer:
      return AccessibilityNodeData::ROLE_DRAWER;
    case WebKit::WebAccessibilityRoleEditableText:
      return AccessibilityNodeData::ROLE_EDITABLE_TEXT;
    case WebKit::WebAccessibilityRoleFooter:
      return AccessibilityNodeData::ROLE_FOOTER;
    case WebKit::WebAccessibilityRoleForm:
      return AccessibilityNodeData::ROLE_FORM;
    case WebKit::WebAccessibilityRoleGrid:
      return AccessibilityNodeData::ROLE_GRID;
    case WebKit::WebAccessibilityRoleGroup:
      return AccessibilityNodeData::ROLE_GROUP;
    case WebKit::WebAccessibilityRoleGrowArea:
      return AccessibilityNodeData::ROLE_GROW_AREA;
    case WebKit::WebAccessibilityRoleHeading:
      return AccessibilityNodeData::ROLE_HEADING;
    case WebKit::WebAccessibilityRoleHelpTag:
      return AccessibilityNodeData::ROLE_HELP_TAG;
    case WebKit::WebAccessibilityRoleHorizontalRule:
      return AccessibilityNodeData::ROLE_HORIZONTAL_RULE;
    case WebKit::WebAccessibilityRoleIgnored:
      return AccessibilityNodeData::ROLE_IGNORED;
    case WebKit::WebAccessibilityRoleImage:
      return AccessibilityNodeData::ROLE_IMAGE;
    case WebKit::WebAccessibilityRoleImageMap:
      return AccessibilityNodeData::ROLE_IMAGE_MAP;
    case WebKit::WebAccessibilityRoleImageMapLink:
      return AccessibilityNodeData::ROLE_IMAGE_MAP_LINK;
    case WebKit::WebAccessibilityRoleIncrementor:
      return AccessibilityNodeData::ROLE_INCREMENTOR;
    case WebKit::WebAccessibilityRoleLabel:
      return AccessibilityNodeData::ROLE_LABEL;
    case WebKit::WebAccessibilityRoleLandmarkApplication:
      return AccessibilityNodeData::ROLE_LANDMARK_APPLICATION;
    case WebKit::WebAccessibilityRoleLandmarkBanner:
      return AccessibilityNodeData::ROLE_LANDMARK_BANNER;
    case WebKit::WebAccessibilityRoleLandmarkComplementary:
      return AccessibilityNodeData::ROLE_LANDMARK_COMPLEMENTARY;
    case WebKit::WebAccessibilityRoleLandmarkContentInfo:
      return AccessibilityNodeData::ROLE_LANDMARK_CONTENTINFO;
    case WebKit::WebAccessibilityRoleLandmarkMain:
      return AccessibilityNodeData::ROLE_LANDMARK_MAIN;
    case WebKit::WebAccessibilityRoleLandmarkNavigation:
      return AccessibilityNodeData::ROLE_LANDMARK_NAVIGATION;
    case WebKit::WebAccessibilityRoleLandmarkSearch:
      return AccessibilityNodeData::ROLE_LANDMARK_SEARCH;
    case WebKit::WebAccessibilityRoleLink:
      return AccessibilityNodeData::ROLE_LINK;
    case WebKit::WebAccessibilityRoleList:
      return AccessibilityNodeData::ROLE_LIST;
    case WebKit::WebAccessibilityRoleListBox:
      return AccessibilityNodeData::ROLE_LISTBOX;
    case WebKit::WebAccessibilityRoleListBoxOption:
      return AccessibilityNodeData::ROLE_LISTBOX_OPTION;
    case WebKit::WebAccessibilityRoleListItem:
      return AccessibilityNodeData::ROLE_LIST_ITEM;
    case WebKit::WebAccessibilityRoleListMarker:
      return AccessibilityNodeData::ROLE_LIST_MARKER;
    case WebKit::WebAccessibilityRoleMatte:
      return AccessibilityNodeData::ROLE_MATTE;
    case WebKit::WebAccessibilityRoleMenu:
      return AccessibilityNodeData::ROLE_MENU;
    case WebKit::WebAccessibilityRoleMenuBar:
      return AccessibilityNodeData::ROLE_MENU_BAR;
    case WebKit::WebAccessibilityRoleMenuButton:
      return AccessibilityNodeData::ROLE_MENU_BUTTON;
    case WebKit::WebAccessibilityRoleMenuItem:
      return AccessibilityNodeData::ROLE_MENU_ITEM;
    case WebKit::WebAccessibilityRoleMenuListOption:
      return AccessibilityNodeData::ROLE_MENU_LIST_OPTION;
    case WebKit::WebAccessibilityRoleMenuListPopup:
      return AccessibilityNodeData::ROLE_MENU_LIST_POPUP;
    case WebKit::WebAccessibilityRoleOutline:
      return AccessibilityNodeData::ROLE_OUTLINE;
    case WebKit::WebAccessibilityRoleParagraph:
      return AccessibilityNodeData::ROLE_PARAGRAPH;
    case WebKit::WebAccessibilityRolePopUpButton:
      return AccessibilityNodeData::ROLE_POPUP_BUTTON;
    case WebKit::WebAccessibilityRolePresentational:
      return AccessibilityNodeData::ROLE_PRESENTATIONAL;
    case WebKit::WebAccessibilityRoleProgressIndicator:
      return AccessibilityNodeData::ROLE_PROGRESS_INDICATOR;
    case WebKit::WebAccessibilityRoleRadioButton:
      return AccessibilityNodeData::ROLE_RADIO_BUTTON;
    case WebKit::WebAccessibilityRoleRadioGroup:
      return AccessibilityNodeData::ROLE_RADIO_GROUP;
    case WebKit::WebAccessibilityRoleRow:
      return AccessibilityNodeData::ROLE_ROW;
    case WebKit::WebAccessibilityRoleRowHeader:
      return AccessibilityNodeData::ROLE_ROW_HEADER;
    case WebKit::WebAccessibilityRoleRuler:
      return AccessibilityNodeData::ROLE_RULER;
    case WebKit::WebAccessibilityRoleRulerMarker:
      return AccessibilityNodeData::ROLE_RULER_MARKER;
    case WebKit::WebAccessibilityRoleScrollArea:
      return AccessibilityNodeData::ROLE_SCROLLAREA;
    case WebKit::WebAccessibilityRoleScrollBar:
      return AccessibilityNodeData::ROLE_SCROLLBAR;
    case WebKit::WebAccessibilityRoleSheet:
      return AccessibilityNodeData::ROLE_SHEET;
    case WebKit::WebAccessibilityRoleSlider:
      return AccessibilityNodeData::ROLE_SLIDER;
    case WebKit::WebAccessibilityRoleSliderThumb:
      return AccessibilityNodeData::ROLE_SLIDER_THUMB;
    case WebKit::WebAccessibilityRoleSpinButton:
      return AccessibilityNodeData::ROLE_SPIN_BUTTON;
    case WebKit::WebAccessibilityRoleSpinButtonPart:
      return AccessibilityNodeData::ROLE_SPIN_BUTTON_PART;
    case WebKit::WebAccessibilityRoleSplitGroup:
      return AccessibilityNodeData::ROLE_SPLIT_GROUP;
    case WebKit::WebAccessibilityRoleSplitter:
      return AccessibilityNodeData::ROLE_SPLITTER;
    case WebKit::WebAccessibilityRoleStaticText:
      return AccessibilityNodeData::ROLE_STATIC_TEXT;
    case WebKit::WebAccessibilityRoleSystemWide:
      return AccessibilityNodeData::ROLE_SYSTEM_WIDE;
    case WebKit::WebAccessibilityRoleTab:
      return AccessibilityNodeData::ROLE_TAB;
    case WebKit::WebAccessibilityRoleTabGroup:
      return AccessibilityNodeData::ROLE_TAB_GROUP_UNUSED;
    case WebKit::WebAccessibilityRoleTabList:
      return AccessibilityNodeData::ROLE_TAB_LIST;
    case WebKit::WebAccessibilityRoleTabPanel:
      return AccessibilityNodeData::ROLE_TAB_PANEL;
    case WebKit::WebAccessibilityRoleTable:
      return AccessibilityNodeData::ROLE_TABLE;
    case WebKit::WebAccessibilityRoleTableHeaderContainer:
      return AccessibilityNodeData::ROLE_TABLE_HEADER_CONTAINER;
    case WebKit::WebAccessibilityRoleTextArea:
      return AccessibilityNodeData::ROLE_TEXTAREA;
    case WebKit::WebAccessibilityRoleTextField:
      return AccessibilityNodeData::ROLE_TEXT_FIELD;
    case WebKit::WebAccessibilityRoleToggleButton:
      return AccessibilityNodeData::ROLE_TOGGLE_BUTTON;
    case WebKit::WebAccessibilityRoleToolbar:
      return AccessibilityNodeData::ROLE_TOOLBAR;
    case WebKit::WebAccessibilityRoleTreeGrid:
      return AccessibilityNodeData::ROLE_TREE_GRID;
    case WebKit::WebAccessibilityRoleTreeItemRole:
      return AccessibilityNodeData::ROLE_TREE_ITEM;
    case WebKit::WebAccessibilityRoleTreeRole:
      return AccessibilityNodeData::ROLE_TREE;
    case WebKit::WebAccessibilityRoleUserInterfaceTooltip:
      return AccessibilityNodeData::ROLE_TOOLTIP;
    case WebKit::WebAccessibilityRoleValueIndicator:
      return AccessibilityNodeData::ROLE_VALUE_INDICATOR;
    case WebKit::WebAccessibilityRoleWebArea:
      return AccessibilityNodeData::ROLE_WEB_AREA;
    case WebKit::WebAccessibilityRoleWebCoreLink:
      return AccessibilityNodeData::ROLE_WEBCORE_LINK;
    case WebKit::WebAccessibilityRoleWindow:
      return AccessibilityNodeData::ROLE_WINDOW;

    default:
      return AccessibilityNodeData::ROLE_UNKNOWN;
  }
}

// Provides a conversion between the WebAccessibilityObject state
// accessors and a state bitmask that can be serialized and sent to the
// Browser process. Rare state are sent as boolean attributes instead.
uint32 ConvertState(const WebAccessibilityObject& o) {
  uint32 state = 0;
  if (o.isChecked())
    state |= (1 << AccessibilityNodeData::STATE_CHECKED);

  if (o.isCollapsed())
    state |= (1 << AccessibilityNodeData::STATE_COLLAPSED);

  if (o.canSetFocusAttribute())
    state |= (1 << AccessibilityNodeData::STATE_FOCUSABLE);

  if (o.isFocused())
    state |= (1 << AccessibilityNodeData::STATE_FOCUSED);

  if (o.roleValue() == WebKit::WebAccessibilityRolePopUpButton ||
      o.ariaHasPopup()) {
    state |= (1 << AccessibilityNodeData::STATE_HASPOPUP);
    if (!o.isCollapsed())
      state |= (1 << AccessibilityNodeData::STATE_EXPANDED);
  }

  if (o.isHovered())
    state |= (1 << AccessibilityNodeData::STATE_HOTTRACKED);

  if (o.isIndeterminate())
    state |= (1 << AccessibilityNodeData::STATE_INDETERMINATE);

  if (!o.isVisible())
    state |= (1 << AccessibilityNodeData::STATE_INVISIBLE);

  if (o.isLinked())
    state |= (1 << AccessibilityNodeData::STATE_LINKED);

  if (o.isMultiSelectable())
    state |= (1 << AccessibilityNodeData::STATE_MULTISELECTABLE);

  if (o.isOffScreen())
    state |= (1 << AccessibilityNodeData::STATE_OFFSCREEN);

  if (o.isPressed())
    state |= (1 << AccessibilityNodeData::STATE_PRESSED);

  if (o.isPasswordField())
    state |= (1 << AccessibilityNodeData::STATE_PROTECTED);

  if (o.isReadOnly())
    state |= (1 << AccessibilityNodeData::STATE_READONLY);

  if (o.isRequired())
    state |= (1 << AccessibilityNodeData::STATE_REQUIRED);

  if (o.canSetSelectedAttribute())
    state |= (1 << AccessibilityNodeData::STATE_SELECTABLE);

  if (o.isSelected())
    state |= (1 << AccessibilityNodeData::STATE_SELECTED);

  if (o.isVisited())
    state |= (1 << AccessibilityNodeData::STATE_TRAVERSED);

  if (!o.isEnabled())
    state |= (1 << AccessibilityNodeData::STATE_UNAVAILABLE);

  if (o.isVertical())
    state |= (1 << AccessibilityNodeData::STATE_VERTICAL);

  if (o.isVisited())
    state |= (1 << AccessibilityNodeData::STATE_VISITED);

  return state;
}

}  // Anonymous namespace

namespace content {

void SerializeAccessibilityNode(
    const WebAccessibilityObject& src,
    AccessibilityNodeData* dst,
    bool include_children) {
  dst->name = src.title();
  dst->role = ConvertRole(src.roleValue());
  dst->state = ConvertState(src);
  dst->location = src.boundingBoxRect();
  dst->id = src.axID();

  if (src.valueDescription().length())
    dst->value = src.valueDescription();
  else
    dst->value = src.stringValue();

  if (src.accessKey().length())
    dst->string_attributes[dst->ATTR_ACCESS_KEY] = src.accessKey();
  if (src.actionVerb().length())
    dst->string_attributes[dst->ATTR_ACTION] = src.actionVerb();
  if (src.isAriaReadOnly())
    dst->bool_attributes[dst->ATTR_ARIA_READONLY] = true;
  if (src.isButtonStateMixed())
    dst->bool_attributes[dst->ATTR_BUTTON_MIXED] = true;
  if (src.canSetValueAttribute())
    dst->bool_attributes[dst->ATTR_CAN_SET_VALUE] = true;
  if (src.accessibilityDescription().length())
    dst->string_attributes[dst->ATTR_DESCRIPTION] =
        src.accessibilityDescription();
  if (src.hasComputedStyle())
    dst->string_attributes[dst->ATTR_DISPLAY] = src.computedStyleDisplay();
  if (src.helpText().length())
    dst->string_attributes[dst->ATTR_HELP] = src.helpText();
  if (src.keyboardShortcut().length())
    dst->string_attributes[dst->ATTR_SHORTCUT] = src.keyboardShortcut();
  if (!src.titleUIElement().isDetached()) {
    dst->int_attributes[dst->ATTR_TITLE_UI_ELEMENT] =
        src.titleUIElement().axID();
  }
  if (!src.url().isEmpty())
    dst->string_attributes[dst->ATTR_URL] = src.url().spec().utf16();

  if (dst->role == dst->ROLE_TREE_ITEM)
    dst->int_attributes[dst->ATTR_HIERARCHICAL_LEVEL] = src.hierarchicalLevel();

  if (dst->role == dst->ROLE_SLIDER)
    include_children = false;

  // Treat the active list box item as focused.
  if (dst->role == dst->ROLE_LISTBOX_OPTION && src.isSelectedOptionActive())
    dst->state |= (1 << AccessibilityNodeData::STATE_FOCUSED);

  if (src.canvasHasFallbackContent())
    dst->role = AccessibilityNodeData::ROLE_CANVAS_WITH_FALLBACK_CONTENT;

  WebNode node = src.node();
  bool is_iframe = false;

  if (!node.isNull() && node.isElementNode()) {
    WebElement element = node.to<WebElement>();
    is_iframe = (element.tagName() == ASCIIToUTF16("IFRAME"));

    // TODO(ctguil): The tagName in WebKit is lower cased but
    // HTMLElement::nodeName calls localNameUpper. Consider adding
    // a WebElement method that returns the original lower cased tagName.
    dst->string_attributes[dst->ATTR_HTML_TAG] =
        StringToLowerASCII(string16(element.tagName()));
    for (unsigned i = 0; i < element.attributeCount(); ++i) {
      string16 name = StringToLowerASCII(string16(
          element.attributeLocalName(i)));
      string16 value = element.attributeValue(i);
      dst->html_attributes.push_back(
          std::pair<string16, string16>(name, value));
    }

    if (dst->role == dst->ROLE_EDITABLE_TEXT ||
        dst->role == dst->ROLE_TEXTAREA ||
        dst->role == dst->ROLE_TEXT_FIELD) {
      // Jaws gets confused by children of text fields, so we ignore them.
      include_children = false;

      dst->int_attributes[dst->ATTR_TEXT_SEL_START] = src.selectionStart();
      dst->int_attributes[dst->ATTR_TEXT_SEL_END] = src.selectionEnd();

      WebVector<int> src_line_breaks;
      src.lineBreaks(src_line_breaks);
      dst->line_breaks.reserve(src_line_breaks.size());
      for (size_t i = 0; i < src_line_breaks.size(); ++i)
        dst->line_breaks.push_back(src_line_breaks[i]);
    }

    // ARIA role.
    if (element.hasAttribute("role")) {
      dst->string_attributes[dst->ATTR_ROLE] = element.getAttribute("role");
    }

    // Live region attributes
    if (element.hasAttribute("aria-atomic")) {
      dst->bool_attributes[dst->ATTR_LIVE_ATOMIC] =
          LowerCaseEqualsASCII(element.getAttribute("aria-atomic"), "true");
    }
    if (element.hasAttribute("aria-busy")) {
      dst->bool_attributes[dst->ATTR_LIVE_BUSY] =
          LowerCaseEqualsASCII(element.getAttribute("aria-busy"), "true");
    }
    if (element.hasAttribute("aria-live")) {
      dst->string_attributes[dst->ATTR_LIVE_STATUS] =
          element.getAttribute("aria-live");
    }
    if (element.hasAttribute("aria-relevant")) {
      dst->string_attributes[dst->ATTR_LIVE_RELEVANT] =
          element.getAttribute("aria-relevant");
    }
  }

  // Walk up the parent chain to set live region attributes of containers

  WebAccessibilityObject container_accessible = src;
  while (!container_accessible.isDetached()) {
    WebNode container_node = container_accessible.node();
    if (!container_node.isNull() && container_node.isElementNode()) {
      WebElement container_elem =
          container_node.to<WebElement>();
      if (container_elem.hasAttribute("aria-atomic") &&
          dst->bool_attributes.find(dst->ATTR_CONTAINER_LIVE_ATOMIC) ==
          dst->bool_attributes.end()) {
        dst->bool_attributes[dst->ATTR_CONTAINER_LIVE_ATOMIC] =
            LowerCaseEqualsASCII(container_elem.getAttribute("aria-atomic"),
                                 "true");
      }
      if (container_elem.hasAttribute("aria-busy") &&
          dst->bool_attributes.find(dst->ATTR_CONTAINER_LIVE_BUSY) ==
          dst->bool_attributes.end()) {
        dst->bool_attributes[dst->ATTR_CONTAINER_LIVE_BUSY] =
            LowerCaseEqualsASCII(container_elem.getAttribute("aria-busy"),
                                 "true");
      }
      if (container_elem.hasAttribute("aria-live") &&
          dst->string_attributes.find(dst->ATTR_CONTAINER_LIVE_STATUS) ==
          dst->string_attributes.end()) {
        dst->string_attributes[dst->ATTR_CONTAINER_LIVE_STATUS] =
            container_elem.getAttribute("aria-live");
      }
      if (container_elem.hasAttribute("aria-relevant") &&
          dst->string_attributes.find(dst->ATTR_CONTAINER_LIVE_RELEVANT) ==
          dst->string_attributes.end()) {
        dst->string_attributes[dst->ATTR_CONTAINER_LIVE_RELEVANT] =
            container_elem.getAttribute("aria-relevant");
      }
    }
    container_accessible = container_accessible.parentObject();
  }

  if (dst->role == dst->ROLE_PROGRESS_INDICATOR ||
      dst->role == dst->ROLE_SCROLLBAR ||
      dst->role == dst->ROLE_SLIDER ||
      dst->role == dst->ROLE_SPIN_BUTTON) {
    dst->float_attributes[dst->ATTR_VALUE_FOR_RANGE] = src.valueForRange();
    dst->float_attributes[dst->ATTR_MAX_VALUE_FOR_RANGE] =
        src.minValueForRange();
    dst->float_attributes[dst->ATTR_MIN_VALUE_FOR_RANGE] =
        src.maxValueForRange();
  }

  if (dst->role == dst->ROLE_DOCUMENT ||
      dst->role == dst->ROLE_WEB_AREA) {
    const WebDocument& document = src.document();
    if (dst->name.empty())
      dst->name = document.title();
    dst->string_attributes[dst->ATTR_DOC_TITLE] = document.title();
    dst->string_attributes[dst->ATTR_DOC_URL] = document.url().spec().utf16();
    dst->string_attributes[dst->ATTR_DOC_MIMETYPE] =
        ASCIIToUTF16(document.isXHTMLDocument() ? "text/xhtml" : "text/html");
    dst->bool_attributes[dst->ATTR_DOC_LOADED] = src.isLoaded();
    dst->float_attributes[dst->ATTR_DOC_LOADING_PROGRESS] =
        src.estimatedLoadingProgress();

    const WebDocumentType& doctype = document.doctype();
    if (!doctype.isNull())
      dst->string_attributes[dst->ATTR_DOC_DOCTYPE] = doctype.name();

    const gfx::Size& scroll_offset = document.frame()->scrollOffset();
    dst->int_attributes[dst->ATTR_SCROLL_X] = scroll_offset.width();
    dst->int_attributes[dst->ATTR_SCROLL_Y] = scroll_offset.height();

    const gfx::Size& min_offset = document.frame()->minimumScrollOffset();
    dst->int_attributes[dst->ATTR_SCROLL_X_MIN] = min_offset.width();
    dst->int_attributes[dst->ATTR_SCROLL_Y_MIN] = min_offset.height();

    const gfx::Size& max_offset = document.frame()->maximumScrollOffset();
    dst->int_attributes[dst->ATTR_SCROLL_X_MAX] = max_offset.width();
    dst->int_attributes[dst->ATTR_SCROLL_Y_MAX] = max_offset.height();
  }

  if (dst->role == dst->ROLE_TABLE) {
    int column_count = src.columnCount();
    int row_count = src.rowCount();
    if (column_count > 0 && row_count > 0) {
      std::set<int> unique_cell_id_set;
      dst->int_attributes[dst->ATTR_TABLE_COLUMN_COUNT] = column_count;
      dst->int_attributes[dst->ATTR_TABLE_ROW_COUNT] = row_count;
      for (int i = 0; i < column_count * row_count; ++i) {
        WebAccessibilityObject cell = src.cellForColumnAndRow(
            i % column_count, i / column_count);
        int cell_id = -1;
        if (!cell.isDetached()) {
          cell_id = cell.axID();
          if (unique_cell_id_set.find(cell_id) == unique_cell_id_set.end()) {
            unique_cell_id_set.insert(cell_id);
            dst->unique_cell_ids.push_back(cell_id);
          }
        }
        dst->cell_ids.push_back(cell_id);
      }
    }
  }

  if (dst->role == dst->ROLE_CELL ||
      dst->role == dst->ROLE_ROW_HEADER ||
      dst->role == dst->ROLE_COLUMN_HEADER) {
    dst->int_attributes[dst->ATTR_TABLE_CELL_COLUMN_INDEX] =
        src.cellColumnIndex();
    dst->int_attributes[dst->ATTR_TABLE_CELL_COLUMN_SPAN] =
        src.cellColumnSpan();
    dst->int_attributes[dst->ATTR_TABLE_CELL_ROW_INDEX] = src.cellRowIndex();
    dst->int_attributes[dst->ATTR_TABLE_CELL_ROW_SPAN] = src.cellRowSpan();
  }

  if (include_children) {
    // Recursively create children.
    int child_count = src.childCount();
    std::set<int32> child_ids;
    for (int i = 0; i < child_count; ++i) {
      WebAccessibilityObject child = src.childAt(i);
      int32 child_id = child.axID();

      // The child may be invalid due to issues in webkit accessibility code.
      // Don't add children that are invalid thus preventing a crash.
      // https://bugs.webkit.org/show_bug.cgi?id=44149
      // TODO(ctguil): We may want to remove this check as webkit stabilizes.
      if (child.isDetached())
        continue;

      // Children may duplicated in the webkit accessibility tree. Only add a
      // child once for the web accessibility tree.
      // https://bugs.webkit.org/show_bug.cgi?id=58930
      if (child_ids.find(child_id) != child_ids.end())
        continue;
      child_ids.insert(child_id);

      // Some nodes appear in the tree in more than one place: for example,
      // a cell in a table appears as a child of both a row and a column.
      // Only recursively add child nodes that have this node as its
      // unignored parent. For child nodes that are actually parented to
      // somethinng else, store only the ID.
      //
      // As an exception, also add children of an iframe element.
      // https://bugs.webkit.org/show_bug.cgi?id=57066
      if (is_iframe || IsParentUnignoredOf(src, child)) {
        dst->children.push_back(AccessibilityNodeData());
        SerializeAccessibilityNode(child,
                                   &dst->children.back(),
                                   include_children);
      } else {
        dst->indirect_child_ids.push_back(child_id);
      }
    }
  }
}

}  // namespace content
