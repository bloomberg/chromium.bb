// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/accessibility_node_serializer.h"

#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDocumentType.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebNode.h"

using blink::WebAXObject;
using blink::WebDocument;
using blink::WebDocumentType;
using blink::WebElement;
using blink::WebNode;
using blink::WebVector;

namespace content {
namespace {

// Returns true if |ancestor| is the first unignored parent of |child|,
// which means that when walking up the parent chain from |child|,
// |ancestor| is the *first* ancestor that isn't marked as
// accessibilityIsIgnored().
bool IsParentUnignoredOf(const WebAXObject& ancestor,
                         const WebAXObject& child) {
  WebAXObject parent = child.parentObject();
  while (!parent.isDetached() && parent.accessibilityIsIgnored())
    parent = parent.parentObject();
  return parent.equals(ancestor);
}

  bool IsTrue(std::string html_value) {
  return LowerCaseEqualsASCII(html_value, "true");
}

// Provides a conversion between the WebAXObject state
// accessors and a state bitmask that can be serialized and sent to the
// Browser process. Rare state are sent as boolean attributes instead.
uint32 ConvertState(const WebAXObject& o) {
  uint32 state = 0;
  if (o.isChecked())
    state |= (1 << blink::WebAXStateChecked);

  if (o.isCollapsed())
    state |= (1 << blink::WebAXStateCollapsed);

  if (o.canSetFocusAttribute())
    state |= (1 << blink::WebAXStateFocusable);

  if (o.isFocused())
    state |= (1 << blink::WebAXStateFocused);

  if (o.role() == blink::WebAXRolePopUpButton ||
      o.ariaHasPopup()) {
    state |= (1 << blink::WebAXStateHaspopup);
    if (!o.isCollapsed())
      state |= (1 << blink::WebAXStateExpanded);
  }

  if (o.isHovered())
    state |= (1 << blink::WebAXStateHovered);

  if (o.isIndeterminate())
    state |= (1 << blink::WebAXStateIndeterminate);

  if (!o.isVisible())
    state |= (1 << blink::WebAXStateInvisible);

  if (o.isLinked())
    state |= (1 << blink::WebAXStateLinked);

  if (o.isMultiSelectable())
    state |= (1 << blink::WebAXStateMultiselectable);

  if (o.isOffScreen())
    state |= (1 << blink::WebAXStateOffscreen);

  if (o.isPressed())
    state |= (1 << blink::WebAXStatePressed);

  if (o.isPasswordField())
    state |= (1 << blink::WebAXStateProtected);

  if (o.isReadOnly())
    state |= (1 << blink::WebAXStateReadonly);

  if (o.isRequired())
    state |= (1 << blink::WebAXStateRequired);

  if (o.canSetSelectedAttribute())
    state |= (1 << blink::WebAXStateSelectable);

  if (o.isSelected())
    state |= (1 << blink::WebAXStateSelected);

  if (o.isVisited())
    state |= (1 << blink::WebAXStateVisited);

  if (o.isEnabled())
    state |= (1 << blink::WebAXStateEnabled);

  if (o.isVertical())
    state |= (1 << blink::WebAXStateVertical);

  if (o.isVisited())
    state |= (1 << blink::WebAXStateVisited);

  return state;
}

}  // Anonymous namespace

void SerializeAccessibilityNode(
    const WebAXObject& src,
    AccessibilityNodeData* dst) {
  dst->role = src.role();
  dst->state = ConvertState(src);
  dst->location = src.boundingBoxRect();
  dst->id = src.axID();
  std::string name = UTF16ToUTF8(src.title());

  std::string value;
  if (src.valueDescription().length()) {
    dst->AddStringAttribute(dst->ATTR_VALUE,
                            UTF16ToUTF8(src.valueDescription()));
  } else {
    dst->AddStringAttribute(dst->ATTR_VALUE, UTF16ToUTF8(src.stringValue()));
  }

  if (dst->role == blink::WebAXRoleColorWell) {
    int r, g, b;
    src.colorValue(r, g, b);
    dst->AddIntAttribute(dst->ATTR_COLOR_VALUE_RED, r);
    dst->AddIntAttribute(dst->ATTR_COLOR_VALUE_GREEN, g);
    dst->AddIntAttribute(dst->ATTR_COLOR_VALUE_BLUE, b);
  }

  if (dst->role == blink::WebAXRoleInlineTextBox) {
    dst->AddIntAttribute(dst->ATTR_TEXT_DIRECTION, src.textDirection());

    WebVector<int> src_character_offsets;
    src.characterOffsets(src_character_offsets);
    std::vector<int32> character_offsets;
    character_offsets.reserve(src_character_offsets.size());
    for (size_t i = 0; i < src_character_offsets.size(); ++i)
      character_offsets.push_back(src_character_offsets[i]);
    dst->AddIntListAttribute(dst->ATTR_CHARACTER_OFFSETS, character_offsets);

    WebVector<int> src_word_starts;
    WebVector<int> src_word_ends;
    src.wordBoundaries(src_word_starts, src_word_ends);
    std::vector<int32> word_starts;
    std::vector<int32> word_ends;
    word_starts.reserve(src_word_starts.size());
    word_ends.reserve(src_word_starts.size());
    for (size_t i = 0; i < src_word_starts.size(); ++i) {
      word_starts.push_back(src_word_starts[i]);
      word_ends.push_back(src_word_ends[i]);
    }
    dst->AddIntListAttribute(dst->ATTR_WORD_STARTS, word_starts);
    dst->AddIntListAttribute(dst->ATTR_WORD_ENDS, word_ends);
  }

  if (src.accessKey().length())
    dst->AddStringAttribute(dst->ATTR_ACCESS_KEY, UTF16ToUTF8(src.accessKey()));
  if (src.actionVerb().length())
    dst->AddStringAttribute(dst->ATTR_ACTION, UTF16ToUTF8(src.actionVerb()));
  if (src.isAriaReadOnly())
    dst->AddBoolAttribute(dst->ATTR_ARIA_READONLY, true);
  if (src.isButtonStateMixed())
    dst->AddBoolAttribute(dst->ATTR_BUTTON_MIXED, true);
  if (src.canSetValueAttribute())
    dst->AddBoolAttribute(dst->ATTR_CAN_SET_VALUE, true);
  if (src.accessibilityDescription().length()) {
    dst->AddStringAttribute(dst->ATTR_DESCRIPTION,
                            UTF16ToUTF8(src.accessibilityDescription()));
  }
  if (src.hasComputedStyle()) {
    dst->AddStringAttribute(dst->ATTR_DISPLAY,
                            UTF16ToUTF8(src.computedStyleDisplay()));
  }
  if (src.helpText().length())
    dst->AddStringAttribute(dst->ATTR_HELP, UTF16ToUTF8(src.helpText()));
  if (src.keyboardShortcut().length()) {
    dst->AddStringAttribute(dst->ATTR_SHORTCUT,
                            UTF16ToUTF8(src.keyboardShortcut()));
  }
  if (!src.titleUIElement().isDetached()) {
    dst->AddIntAttribute(dst->ATTR_TITLE_UI_ELEMENT,
                         src.titleUIElement().axID());
  }
  if (!src.url().isEmpty())
    dst->AddStringAttribute(dst->ATTR_URL, src.url().spec());

  if (dst->role == blink::WebAXRoleHeading)
    dst->AddIntAttribute(dst->ATTR_HIERARCHICAL_LEVEL, src.headingLevel());
  else if ((dst->role == blink::WebAXRoleTreeItem ||
            dst->role == blink::WebAXRoleRow) &&
           src.hierarchicalLevel() > 0) {
    dst->AddIntAttribute(dst->ATTR_HIERARCHICAL_LEVEL, src.hierarchicalLevel());
  }

  // Treat the active list box item as focused.
  if (dst->role == blink::WebAXRoleListBoxOption &&
      src.isSelectedOptionActive()) {
    dst->state |= (1 << blink::WebAXStateFocused);
  }

  if (src.canvasHasFallbackContent())
    dst->AddBoolAttribute(dst->ATTR_CANVAS_HAS_FALLBACK, true);

  WebNode node = src.node();
  bool is_iframe = false;
  std::string live_atomic;
  std::string live_busy;
  std::string live_status;
  std::string live_relevant;

  if (!node.isNull() && node.isElementNode()) {
    WebElement element = node.to<WebElement>();
    is_iframe = (element.tagName() == ASCIIToUTF16("IFRAME"));

    if (LowerCaseEqualsASCII(element.getAttribute("aria-expanded"), "true"))
      dst->state |= (1 << blink::WebAXStateExpanded);

    // TODO(ctguil): The tagName in WebKit is lower cased but
    // HTMLElement::nodeName calls localNameUpper. Consider adding
    // a WebElement method that returns the original lower cased tagName.
    dst->AddStringAttribute(
        dst->ATTR_HTML_TAG,
        StringToLowerASCII(UTF16ToUTF8(element.tagName())));
    for (unsigned i = 0; i < element.attributeCount(); ++i) {
      std::string name = StringToLowerASCII(UTF16ToUTF8(
          element.attributeLocalName(i)));
      std::string value = UTF16ToUTF8(element.attributeValue(i));
      dst->html_attributes.push_back(std::make_pair(name, value));
    }

    if (dst->role == blink::WebAXRoleEditableText ||
        dst->role == blink::WebAXRoleTextArea ||
        dst->role == blink::WebAXRoleTextField) {
      dst->AddIntAttribute(dst->ATTR_TEXT_SEL_START, src.selectionStart());
      dst->AddIntAttribute(dst->ATTR_TEXT_SEL_END, src.selectionEnd());

      WebVector<int> src_line_breaks;
      src.lineBreaks(src_line_breaks);
      if (src_line_breaks.size() > 0) {
        std::vector<int32> line_breaks;
        line_breaks.reserve(src_line_breaks.size());
        for (size_t i = 0; i < src_line_breaks.size(); ++i)
          line_breaks.push_back(src_line_breaks[i]);
        dst->AddIntListAttribute(dst->ATTR_LINE_BREAKS, line_breaks);
      }
    }

    // ARIA role.
    if (element.hasAttribute("role")) {
      dst->AddStringAttribute(dst->ATTR_ROLE,
                              UTF16ToUTF8(element.getAttribute("role")));
    }

    // Live region attributes
    live_atomic = UTF16ToUTF8(element.getAttribute("aria-atomic"));
    live_busy = UTF16ToUTF8(element.getAttribute("aria-busy"));
    live_status = UTF16ToUTF8(element.getAttribute("aria-live"));
    live_relevant = UTF16ToUTF8(element.getAttribute("aria-relevant"));
  }

  // Walk up the parent chain to set live region attributes of containers
  std::string container_live_atomic;
  std::string container_live_busy;
  std::string container_live_status;
  std::string container_live_relevant;
  WebAXObject container_accessible = src;
  while (!container_accessible.isDetached()) {
    WebNode container_node = container_accessible.node();
    if (!container_node.isNull() && container_node.isElementNode()) {
      WebElement container_elem = container_node.to<WebElement>();
      if (container_elem.hasAttribute("aria-atomic") &&
          container_live_atomic.empty()) {
        container_live_atomic =
            UTF16ToUTF8(container_elem.getAttribute("aria-atomic"));
      }
      if (container_elem.hasAttribute("aria-busy") &&
          container_live_busy.empty()) {
        container_live_busy =
            UTF16ToUTF8(container_elem.getAttribute("aria-busy"));
      }
      if (container_elem.hasAttribute("aria-live") &&
          container_live_status.empty()) {
        container_live_status =
            UTF16ToUTF8(container_elem.getAttribute("aria-live"));
      }
      if (container_elem.hasAttribute("aria-relevant") &&
          container_live_relevant.empty()) {
        container_live_relevant =
            UTF16ToUTF8(container_elem.getAttribute("aria-relevant"));
      }
    }
    container_accessible = container_accessible.parentObject();
  }

  if (!live_atomic.empty())
    dst->AddBoolAttribute(dst->ATTR_LIVE_ATOMIC, IsTrue(live_atomic));
  if (!live_busy.empty())
    dst->AddBoolAttribute(dst->ATTR_LIVE_BUSY, IsTrue(live_busy));
  if (!live_status.empty())
    dst->AddStringAttribute(dst->ATTR_LIVE_STATUS, live_status);
  if (!live_relevant.empty())
    dst->AddStringAttribute(dst->ATTR_LIVE_RELEVANT, live_relevant);

  if (!container_live_atomic.empty()) {
    dst->AddBoolAttribute(dst->ATTR_CONTAINER_LIVE_ATOMIC,
                          IsTrue(container_live_atomic));
  }
  if (!container_live_busy.empty()) {
    dst->AddBoolAttribute(dst->ATTR_CONTAINER_LIVE_BUSY,
                          IsTrue(container_live_busy));
  }
  if (!container_live_status.empty()) {
    dst->AddStringAttribute(dst->ATTR_CONTAINER_LIVE_STATUS,
                            container_live_status);
  }
  if (!container_live_relevant.empty()) {
    dst->AddStringAttribute(dst->ATTR_CONTAINER_LIVE_RELEVANT,
                            container_live_relevant);
  }

  if (dst->role == blink::WebAXRoleProgressIndicator ||
      dst->role == blink::WebAXRoleScrollBar ||
      dst->role == blink::WebAXRoleSlider ||
      dst->role == blink::WebAXRoleSpinButton) {
    dst->AddFloatAttribute(dst->ATTR_VALUE_FOR_RANGE, src.valueForRange());
    dst->AddFloatAttribute(dst->ATTR_MAX_VALUE_FOR_RANGE,
                           src.maxValueForRange());
    dst->AddFloatAttribute(dst->ATTR_MIN_VALUE_FOR_RANGE,
                           src.minValueForRange());
  }

  if (dst->role == blink::WebAXRoleDocument ||
      dst->role == blink::WebAXRoleWebArea) {
    dst->AddStringAttribute(dst->ATTR_HTML_TAG, "#document");
    const WebDocument& document = src.document();
    if (name.empty())
      name = UTF16ToUTF8(document.title());
    dst->AddStringAttribute(dst->ATTR_DOC_TITLE, UTF16ToUTF8(document.title()));
    dst->AddStringAttribute(dst->ATTR_DOC_URL, document.url().spec());
    dst->AddStringAttribute(
        dst->ATTR_DOC_MIMETYPE,
        document.isXHTMLDocument() ? "text/xhtml" : "text/html");
    dst->AddBoolAttribute(dst->ATTR_DOC_LOADED, src.isLoaded());
    dst->AddFloatAttribute(dst->ATTR_DOC_LOADING_PROGRESS,
                           src.estimatedLoadingProgress());

    const WebDocumentType& doctype = document.doctype();
    if (!doctype.isNull()) {
      dst->AddStringAttribute(dst->ATTR_DOC_DOCTYPE,
                              UTF16ToUTF8(doctype.name()));
    }

    const gfx::Size& scroll_offset = document.frame()->scrollOffset();
    dst->AddIntAttribute(dst->ATTR_SCROLL_X, scroll_offset.width());
    dst->AddIntAttribute(dst->ATTR_SCROLL_Y, scroll_offset.height());

    const gfx::Size& min_offset = document.frame()->minimumScrollOffset();
    dst->AddIntAttribute(dst->ATTR_SCROLL_X_MIN, min_offset.width());
    dst->AddIntAttribute(dst->ATTR_SCROLL_Y_MIN, min_offset.height());

    const gfx::Size& max_offset = document.frame()->maximumScrollOffset();
    dst->AddIntAttribute(dst->ATTR_SCROLL_X_MAX, max_offset.width());
    dst->AddIntAttribute(dst->ATTR_SCROLL_Y_MAX, max_offset.height());
  }

  if (dst->role == blink::WebAXRoleTable) {
    int column_count = src.columnCount();
    int row_count = src.rowCount();
    if (column_count > 0 && row_count > 0) {
      std::set<int32> unique_cell_id_set;
      std::vector<int32> cell_ids;
      std::vector<int32> unique_cell_ids;
      dst->AddIntAttribute(dst->ATTR_TABLE_COLUMN_COUNT, column_count);
      dst->AddIntAttribute(dst->ATTR_TABLE_ROW_COUNT, row_count);
      WebAXObject header = src.headerContainerObject();
      if (!header.isDetached())
        dst->AddIntAttribute(dst->ATTR_TABLE_HEADER_ID, header.axID());
      for (int i = 0; i < column_count * row_count; ++i) {
        WebAXObject cell = src.cellForColumnAndRow(
            i % column_count, i / column_count);
        int cell_id = -1;
        if (!cell.isDetached()) {
          cell_id = cell.axID();
          if (unique_cell_id_set.find(cell_id) == unique_cell_id_set.end()) {
            unique_cell_id_set.insert(cell_id);
            unique_cell_ids.push_back(cell_id);
          }
        }
        cell_ids.push_back(cell_id);
      }
      dst->AddIntListAttribute(dst->ATTR_CELL_IDS, cell_ids);
      dst->AddIntListAttribute(dst->ATTR_UNIQUE_CELL_IDS, unique_cell_ids);
    }
  }

  if (dst->role == blink::WebAXRoleRow) {
    dst->AddIntAttribute(dst->ATTR_TABLE_ROW_INDEX, src.rowIndex());
    WebAXObject header = src.rowHeader();
    if (!header.isDetached())
      dst->AddIntAttribute(dst->ATTR_TABLE_ROW_HEADER_ID, header.axID());
  }

  if (dst->role == blink::WebAXRoleColumn) {
    dst->AddIntAttribute(dst->ATTR_TABLE_COLUMN_INDEX, src.columnIndex());
    WebAXObject header = src.columnHeader();
    if (!header.isDetached())
      dst->AddIntAttribute(dst->ATTR_TABLE_COLUMN_HEADER_ID, header.axID());
  }

  if (dst->role == blink::WebAXRoleCell ||
      dst->role == blink::WebAXRoleRowHeader ||
      dst->role == blink::WebAXRoleColumnHeader) {
    dst->AddIntAttribute(dst->ATTR_TABLE_CELL_COLUMN_INDEX,
                         src.cellColumnIndex());
    dst->AddIntAttribute(dst->ATTR_TABLE_CELL_COLUMN_SPAN,
                         src.cellColumnSpan());
    dst->AddIntAttribute(dst->ATTR_TABLE_CELL_ROW_INDEX, src.cellRowIndex());
    dst->AddIntAttribute(dst->ATTR_TABLE_CELL_ROW_SPAN, src.cellRowSpan());
  }

  dst->AddStringAttribute(dst->ATTR_NAME, name);

  // Add the ids of *indirect* children - those who are children of this node,
  // but whose parent is *not* this node. One example is a table
  // cell, which is a child of both a row and a column. Because the cell's
  // parent is the row, the row adds it as a child, and the column adds it
  // as an indirect child.
  int child_count = src.childCount();
  for (int i = 0; i < child_count; ++i) {
    WebAXObject child = src.childAt(i);
    std::vector<int32> indirect_child_ids;
    if (!is_iframe && !child.isDetached() && !IsParentUnignoredOf(src, child))
      indirect_child_ids.push_back(child.axID());
    if (indirect_child_ids.size() > 0) {
      dst->AddIntListAttribute(
          dst->ATTR_INDIRECT_CHILD_IDS, indirect_child_ids);
    }
  }
}

bool ShouldIncludeChildNode(
    const WebAXObject& parent,
    const WebAXObject& child) {
  switch(parent.role()) {
    case blink::WebAXRoleSlider:
    case blink::WebAXRoleEditableText:
    case blink::WebAXRoleTextArea:
    case blink::WebAXRoleTextField:
      return false;
    default:
      break;
  }

  // The child may be invalid due to issues in webkit accessibility code.
  // Don't add children that are invalid thus preventing a crash.
  // https://bugs.webkit.org/show_bug.cgi?id=44149
  // TODO(ctguil): We may want to remove this check as webkit stabilizes.
  if (child.isDetached())
    return false;

  // Skip children whose parent isn't this - see indirect_child_ids, above.
  // As an exception, include children of an iframe element.
  bool is_iframe = false;
  WebNode node = parent.node();
  if (!node.isNull() && node.isElementNode()) {
    WebElement element = node.to<WebElement>();
    is_iframe = (element.tagName() == ASCIIToUTF16("IFRAME"));
  }

  return (is_iframe || IsParentUnignoredOf(parent, child));
}

}  // namespace content
