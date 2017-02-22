// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_ax_object_proxy.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gin/handle.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace test_runner {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
std::string RoleToString(blink::WebAXRole role) {
  std::string result = "AXRole: AX";
  switch (role) {
    case blink::WebAXRoleAbbr:
      return result.append("Abbr");
    case blink::WebAXRoleAlertDialog:
      return result.append("AlertDialog");
    case blink::WebAXRoleAlert:
      return result.append("Alert");
    case blink::WebAXRoleAnnotation:
      return result.append("Annotation");
    case blink::WebAXRoleApplication:
      return result.append("Application");
    case blink::WebAXRoleArticle:
      return result.append("Article");
    case blink::WebAXRoleAudio:
      return result.append("Audio");
    case blink::WebAXRoleBanner:
      return result.append("Banner");
    case blink::WebAXRoleBlockquote:
      return result.append("Blockquote");
    case blink::WebAXRoleBusyIndicator:
      return result.append("BusyIndicator");
    case blink::WebAXRoleButton:
      return result.append("Button");
    case blink::WebAXRoleCanvas:
      return result.append("Canvas");
    case blink::WebAXRoleCaption:
      return result.append("Caption");
    case blink::WebAXRoleCell:
      return result.append("Cell");
    case blink::WebAXRoleCheckBox:
      return result.append("CheckBox");
    case blink::WebAXRoleColorWell:
      return result.append("ColorWell");
    case blink::WebAXRoleColumnHeader:
      return result.append("ColumnHeader");
    case blink::WebAXRoleColumn:
      return result.append("Column");
    case blink::WebAXRoleComboBox:
      return result.append("ComboBox");
    case blink::WebAXRoleComplementary:
      return result.append("Complementary");
    case blink::WebAXRoleContentInfo:
      return result.append("ContentInfo");
    case blink::WebAXRoleDate:
      return result.append("DateField");
    case blink::WebAXRoleDateTime:
      return result.append("DateTimeField");
    case blink::WebAXRoleDefinition:
      return result.append("Definition");
    case blink::WebAXRoleDescriptionListDetail:
      return result.append("DescriptionListDetail");
    case blink::WebAXRoleDescriptionList:
      return result.append("DescriptionList");
    case blink::WebAXRoleDescriptionListTerm:
      return result.append("DescriptionListTerm");
    case blink::WebAXRoleDetails:
      return result.append("Details");
    case blink::WebAXRoleDialog:
      return result.append("Dialog");
    case blink::WebAXRoleDirectory:
      return result.append("Directory");
    case blink::WebAXRoleDisclosureTriangle:
      return result.append("DisclosureTriangle");
    case blink::WebAXRoleDiv:
      return result.append("Div");
    case blink::WebAXRoleDocument:
      return result.append("Document");
    case blink::WebAXRoleEmbeddedObject:
      return result.append("EmbeddedObject");
    case blink::WebAXRoleFigcaption:
      return result.append("Figcaption");
    case blink::WebAXRoleFigure:
      return result.append("Figure");
    case blink::WebAXRoleFooter:
      return result.append("Footer");
    case blink::WebAXRoleForm:
      return result.append("Form");
    case blink::WebAXRoleGrid:
      return result.append("Grid");
    case blink::WebAXRoleGroup:
      return result.append("Group");
    case blink::WebAXRoleHeading:
      return result.append("Heading");
    case blink::WebAXRoleIgnored:
      return result.append("Ignored");
    case blink::WebAXRoleImageMapLink:
      return result.append("ImageMapLink");
    case blink::WebAXRoleImageMap:
      return result.append("ImageMap");
    case blink::WebAXRoleImage:
      return result.append("Image");
    case blink::WebAXRoleInlineTextBox:
      return result.append("InlineTextBox");
    case blink::WebAXRoleInputTime:
      return result.append("InputTime");
    case blink::WebAXRoleLabel:
      return result.append("Label");
    case blink::WebAXRoleLegend:
      return result.append("Legend");
    case blink::WebAXRoleLink:
      return result.append("Link");
    case blink::WebAXRoleLineBreak:
      return result.append("LineBreak");
    case blink::WebAXRoleListBoxOption:
      return result.append("ListBoxOption");
    case blink::WebAXRoleListBox:
      return result.append("ListBox");
    case blink::WebAXRoleListItem:
      return result.append("ListItem");
    case blink::WebAXRoleListMarker:
      return result.append("ListMarker");
    case blink::WebAXRoleList:
      return result.append("List");
    case blink::WebAXRoleLog:
      return result.append("Log");
    case blink::WebAXRoleMain:
      return result.append("Main");
    case blink::WebAXRoleMark:
      return result.append("Mark");
    case blink::WebAXRoleMarquee:
      return result.append("Marquee");
    case blink::WebAXRoleMath:
      return result.append("Math");
    case blink::WebAXRoleMenuBar:
      return result.append("MenuBar");
    case blink::WebAXRoleMenuButton:
      return result.append("MenuButton");
    case blink::WebAXRoleMenuItem:
      return result.append("MenuItem");
    case blink::WebAXRoleMenuItemCheckBox:
      return result.append("MenuItemCheckBox");
    case blink::WebAXRoleMenuItemRadio:
      return result.append("MenuItemRadio");
    case blink::WebAXRoleMenuListOption:
      return result.append("MenuListOption");
    case blink::WebAXRoleMenuListPopup:
      return result.append("MenuListPopup");
    case blink::WebAXRoleMenu:
      return result.append("Menu");
    case blink::WebAXRoleMeter:
      return result.append("Meter");
    case blink::WebAXRoleNavigation:
      return result.append("Navigation");
    case blink::WebAXRoleNone:
      return result.append("None");
    case blink::WebAXRoleNote:
      return result.append("Note");
    case blink::WebAXRoleOutline:
      return result.append("Outline");
    case blink::WebAXRoleParagraph:
      return result.append("Paragraph");
    case blink::WebAXRolePopUpButton:
      return result.append("PopUpButton");
    case blink::WebAXRolePre:
      return result.append("Pre");
    case blink::WebAXRolePresentational:
      return result.append("Presentational");
    case blink::WebAXRoleProgressIndicator:
      return result.append("ProgressIndicator");
    case blink::WebAXRoleRadioButton:
      return result.append("RadioButton");
    case blink::WebAXRoleRadioGroup:
      return result.append("RadioGroup");
    case blink::WebAXRoleRegion:
      return result.append("Region");
    case blink::WebAXRoleRootWebArea:
      return result.append("RootWebArea");
    case blink::WebAXRoleRowHeader:
      return result.append("RowHeader");
    case blink::WebAXRoleRow:
      return result.append("Row");
    case blink::WebAXRoleRuby:
      return result.append("Ruby");
    case blink::WebAXRoleRuler:
      return result.append("Ruler");
    case blink::WebAXRoleSVGRoot:
      return result.append("SVGRoot");
    case blink::WebAXRoleScrollArea:
      return result.append("ScrollArea");
    case blink::WebAXRoleScrollBar:
      return result.append("ScrollBar");
    case blink::WebAXRoleSeamlessWebArea:
      return result.append("SeamlessWebArea");
    case blink::WebAXRoleSearch:
      return result.append("Search");
    case blink::WebAXRoleSearchBox:
      return result.append("SearchBox");
    case blink::WebAXRoleSlider:
      return result.append("Slider");
    case blink::WebAXRoleSliderThumb:
      return result.append("SliderThumb");
    case blink::WebAXRoleSpinButtonPart:
      return result.append("SpinButtonPart");
    case blink::WebAXRoleSpinButton:
      return result.append("SpinButton");
    case blink::WebAXRoleSplitter:
      return result.append("Splitter");
    case blink::WebAXRoleStaticText:
      return result.append("StaticText");
    case blink::WebAXRoleStatus:
      return result.append("Status");
    case blink::WebAXRoleSwitch:
      return result.append("Switch");
    case blink::WebAXRoleTabGroup:
      return result.append("TabGroup");
    case blink::WebAXRoleTabList:
      return result.append("TabList");
    case blink::WebAXRoleTabPanel:
      return result.append("TabPanel");
    case blink::WebAXRoleTab:
      return result.append("Tab");
    case blink::WebAXRoleTableHeaderContainer:
      return result.append("TableHeaderContainer");
    case blink::WebAXRoleTable:
      return result.append("Table");
    case blink::WebAXRoleTextField:
      return result.append("TextField");
    case blink::WebAXRoleTime:
      return result.append("Time");
    case blink::WebAXRoleTimer:
      return result.append("Timer");
    case blink::WebAXRoleToggleButton:
      return result.append("ToggleButton");
    case blink::WebAXRoleToolbar:
      return result.append("Toolbar");
    case blink::WebAXRoleTreeGrid:
      return result.append("TreeGrid");
    case blink::WebAXRoleTreeItem:
      return result.append("TreeItem");
    case blink::WebAXRoleTree:
      return result.append("Tree");
    case blink::WebAXRoleUnknown:
      return result.append("Unknown");
    case blink::WebAXRoleUserInterfaceTooltip:
      return result.append("UserInterfaceTooltip");
    case blink::WebAXRoleVideo:
      return result.append("Video");
    case blink::WebAXRoleWebArea:
      return result.append("WebArea");
    case blink::WebAXRoleWindow:
      return result.append("Window");
    default:
      return result.append("Unknown");
  }
}

std::string GetStringValue(const blink::WebAXObject& object) {
  std::string value;
  if (object.role() == blink::WebAXRoleColorWell) {
    unsigned int color = object.colorValue();
    unsigned int red = (color >> 16) & 0xFF;
    unsigned int green = (color >> 8) & 0xFF;
    unsigned int blue = color & 0xFF;
    value = base::StringPrintf("rgba(%d, %d, %d, 1)", red, green, blue);
  } else {
    value = object.stringValue().utf8();
  }
  return value.insert(0, "AXValue: ");
}

std::string GetRole(const blink::WebAXObject& object) {
  std::string role_string = RoleToString(object.role());

  // Special-case canvas with fallback content because Chromium wants to treat
  // this as essentially a separate role that it can map differently depending
  // on the platform.
  if (object.role() == blink::WebAXRoleCanvas &&
      object.canvasHasFallbackContent()) {
    role_string += "WithFallbackContent";
  }

  return role_string;
}

std::string GetValueDescription(const blink::WebAXObject& object) {
  std::string value_description = object.valueDescription().utf8();
  return value_description.insert(0, "AXValueDescription: ");
}

std::string GetLanguage(const blink::WebAXObject& object) {
  std::string language = object.language().utf8();
  return language.insert(0, "AXLanguage: ");
}

std::string GetAttributes(const blink::WebAXObject& object) {
  std::string attributes(object.name().utf8());
  attributes.append("\n");
  attributes.append(GetRole(object));
  return attributes;
}

// New bounds calculation algorithm.  Retrieves the frame-relative bounds
// of an object by calling getRelativeBounds and then applying the offsets
// and transforms recursively on each container of this object.
blink::WebFloatRect BoundsForObject(const blink::WebAXObject& object) {
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  object.getRelativeBounds(container, bounds, matrix);
  gfx::RectF computedBounds(0, 0, bounds.width, bounds.height);
  while (!container.isDetached()) {
    computedBounds.Offset(bounds.x, bounds.y);
    computedBounds.Offset(-container.getScrollOffset().x,
                          -container.getScrollOffset().y);
    if (!matrix.isIdentity()) {
      gfx::Transform transform(matrix);
      transform.TransformRect(&computedBounds);
    }
    container.getRelativeBounds(container, bounds, matrix);
  }
  return blink::WebFloatRect(computedBounds.x(), computedBounds.y(),
                             computedBounds.width(), computedBounds.height());
}

blink::WebRect BoundsForCharacter(const blink::WebAXObject& object,
                                  int characterIndex) {
  DCHECK_EQ(object.role(), blink::WebAXRoleStaticText);
  int end = 0;
  for (unsigned i = 0; i < object.childCount(); i++) {
    blink::WebAXObject inline_text_box = object.childAt(i);
    DCHECK_EQ(inline_text_box.role(), blink::WebAXRoleInlineTextBox);
    int start = end;
    blink::WebString name = inline_text_box.name();
    end += name.length();
    if (characterIndex < start || characterIndex >= end)
      continue;

    blink::WebFloatRect inline_text_box_rect = BoundsForObject(inline_text_box);

    int localIndex = characterIndex - start;
    blink::WebVector<int> character_offsets;
    inline_text_box.characterOffsets(character_offsets);
    if (character_offsets.size() != name.length())
      return blink::WebRect();

    switch (inline_text_box.textDirection()) {
      case blink::WebAXTextDirectionLR: {
        if (localIndex) {
          int left = inline_text_box_rect.x + character_offsets[localIndex - 1];
          int width =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(left, inline_text_box_rect.y, width,
                                inline_text_box_rect.height);
        }
        return blink::WebRect(inline_text_box_rect.x, inline_text_box_rect.y,
                              character_offsets[0],
                              inline_text_box_rect.height);
      }
      case blink::WebAXTextDirectionRL: {
        int right = inline_text_box_rect.x + inline_text_box_rect.width;

        if (localIndex) {
          int left = right - character_offsets[localIndex];
          int width =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(left, inline_text_box_rect.y, width,
                                inline_text_box_rect.height);
        }
        int left = right - character_offsets[0];
        return blink::WebRect(left, inline_text_box_rect.y,
                              character_offsets[0],
                              inline_text_box_rect.height);
      }
      case blink::WebAXTextDirectionTB: {
        if (localIndex) {
          int top = inline_text_box_rect.y + character_offsets[localIndex - 1];
          int height =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(inline_text_box_rect.x, top,
                                inline_text_box_rect.width, height);
        }
        return blink::WebRect(inline_text_box_rect.x, inline_text_box_rect.y,
                              inline_text_box_rect.width, character_offsets[0]);
      }
      case blink::WebAXTextDirectionBT: {
        int bottom = inline_text_box_rect.y + inline_text_box_rect.height;

        if (localIndex) {
          int top = bottom - character_offsets[localIndex];
          int height =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(inline_text_box_rect.x, top,
                                inline_text_box_rect.width, height);
        }
        int top = bottom - character_offsets[0];
        return blink::WebRect(inline_text_box_rect.x, top,
                              inline_text_box_rect.width, character_offsets[0]);
      }
    }
  }

  DCHECK(false);
  return blink::WebRect();
}

std::vector<std::string> GetMisspellings(blink::WebAXObject& object) {
  std::vector<std::string> misspellings;
  std::string text(object.name().utf8());

  blink::WebVector<blink::WebAXMarkerType> marker_types;
  blink::WebVector<int> marker_starts;
  blink::WebVector<int> marker_ends;
  object.markers(marker_types, marker_starts, marker_ends);
  DCHECK_EQ(marker_types.size(), marker_starts.size());
  DCHECK_EQ(marker_starts.size(), marker_ends.size());

  for (size_t i = 0; i < marker_types.size(); ++i) {
    if (marker_types[i] & blink::WebAXMarkerTypeSpelling) {
      misspellings.push_back(
          text.substr(marker_starts[i], marker_ends[i] - marker_starts[i]));
    }
  }

  return misspellings;
}

void GetBoundariesForOneWord(const blink::WebAXObject& object,
                             int character_index,
                             int& word_start,
                             int& word_end) {
  int end = 0;
  for (size_t i = 0; i < object.childCount(); i++) {
    blink::WebAXObject inline_text_box = object.childAt(i);
    DCHECK_EQ(inline_text_box.role(), blink::WebAXRoleInlineTextBox);
    int start = end;
    blink::WebString name = inline_text_box.name();
    end += name.length();
    if (end <= character_index)
      continue;
    int localIndex = character_index - start;

    blink::WebVector<int> starts;
    blink::WebVector<int> ends;
    inline_text_box.wordBoundaries(starts, ends);
    size_t word_count = starts.size();
    DCHECK_EQ(ends.size(), word_count);

    // If there are no words, use the InlineTextBox boundaries.
    if (!word_count) {
      word_start = start;
      word_end = end;
      return;
    }

    // Look for a character within any word other than the last.
    for (size_t j = 0; j < word_count - 1; j++) {
      if (localIndex <= ends[j]) {
        word_start = start + starts[j];
        word_end = start + ends[j];
        return;
      }
    }

    // Return the last word by default.
    word_start = start + starts[word_count - 1];
    word_end = start + ends[word_count - 1];
    return;
  }
}

// Collects attributes into a string, delimited by dashes. Used by all methods
// that output lists of attributes: attributesOfLinkedUIElementsCallback,
// AttributesOfChildrenCallback, etc.
class AttributesCollector {
 public:
  AttributesCollector() {}
  ~AttributesCollector() {}

  void CollectAttributes(const blink::WebAXObject& object) {
    attributes_.append("\n------------\n");
    attributes_.append(GetAttributes(object));
  }

  std::string attributes() const { return attributes_; }

 private:
  std::string attributes_;

  DISALLOW_COPY_AND_ASSIGN(AttributesCollector);
};

class SparseAttributeAdapter : public blink::WebAXSparseAttributeClient {
 public:
  SparseAttributeAdapter() {}
  ~SparseAttributeAdapter() override {}

  std::map<blink::WebAXBoolAttribute, bool> bool_attributes;
  std::map<blink::WebAXStringAttribute, blink::WebString> string_attributes;
  std::map<blink::WebAXObjectAttribute, blink::WebAXObject> object_attributes;
  std::map<blink::WebAXObjectVectorAttribute,
           blink::WebVector<blink::WebAXObject>>
      object_vector_attributes;

 private:
  void addBoolAttribute(blink::WebAXBoolAttribute attribute,
                        bool value) override {
    bool_attributes[attribute] = value;
  }

  void addStringAttribute(blink::WebAXStringAttribute attribute,
                          const blink::WebString& value) override {
    string_attributes[attribute] = value;
  }

  void addObjectAttribute(blink::WebAXObjectAttribute attribute,
                          const blink::WebAXObject& value) override {
    object_attributes[attribute] = value;
  }

  void addObjectVectorAttribute(
      blink::WebAXObjectVectorAttribute attribute,
      const blink::WebVector<blink::WebAXObject>& value) override {
    object_vector_attributes[attribute] = value;
  }
};

}  // namespace

gin::WrapperInfo WebAXObjectProxy::kWrapperInfo = {gin::kEmbedderNativeGin};

WebAXObjectProxy::WebAXObjectProxy(const blink::WebAXObject& object,
                                   WebAXObjectProxy::Factory* factory)
    : accessibility_object_(object), factory_(factory) {}

WebAXObjectProxy::~WebAXObjectProxy() {}

gin::ObjectTemplateBuilder WebAXObjectProxy::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<WebAXObjectProxy>::GetObjectTemplateBuilder(isolate)
      .SetProperty("role", &WebAXObjectProxy::Role)
      .SetProperty("stringValue", &WebAXObjectProxy::StringValue)
      .SetProperty("language", &WebAXObjectProxy::Language)
      .SetProperty("x", &WebAXObjectProxy::X)
      .SetProperty("y", &WebAXObjectProxy::Y)
      .SetProperty("width", &WebAXObjectProxy::Width)
      .SetProperty("height", &WebAXObjectProxy::Height)
      .SetProperty("intValue", &WebAXObjectProxy::IntValue)
      .SetProperty("minValue", &WebAXObjectProxy::MinValue)
      .SetProperty("maxValue", &WebAXObjectProxy::MaxValue)
      .SetProperty("valueDescription", &WebAXObjectProxy::ValueDescription)
      .SetProperty("childrenCount", &WebAXObjectProxy::ChildrenCount)
      .SetProperty("selectionAnchorObject",
                   &WebAXObjectProxy::SelectionAnchorObject)
      .SetProperty("selectionAnchorOffset",
                   &WebAXObjectProxy::SelectionAnchorOffset)
      .SetProperty("selectionAnchorAffinity",
                   &WebAXObjectProxy::SelectionAnchorAffinity)
      .SetProperty("selectionFocusObject",
                   &WebAXObjectProxy::SelectionFocusObject)
      .SetProperty("selectionFocusOffset",
                   &WebAXObjectProxy::SelectionFocusOffset)
      .SetProperty("selectionFocusAffinity",
                   &WebAXObjectProxy::SelectionFocusAffinity)
      .SetProperty("selectionStart", &WebAXObjectProxy::SelectionStart)
      .SetProperty("selectionEnd", &WebAXObjectProxy::SelectionEnd)
      .SetProperty("selectionStartLineNumber",
                   &WebAXObjectProxy::SelectionStartLineNumber)
      .SetProperty("selectionEndLineNumber",
                   &WebAXObjectProxy::SelectionEndLineNumber)
      .SetProperty("isEnabled", &WebAXObjectProxy::IsEnabled)
      .SetProperty("isRequired", &WebAXObjectProxy::IsRequired)
      .SetProperty("isEditable", &WebAXObjectProxy::IsEditable)
      .SetProperty("isRichlyEditable", &WebAXObjectProxy::IsRichlyEditable)
      .SetProperty("isFocused", &WebAXObjectProxy::IsFocused)
      .SetProperty("isFocusable", &WebAXObjectProxy::IsFocusable)
      .SetProperty("isModal", &WebAXObjectProxy::IsModal)
      .SetProperty("isSelected", &WebAXObjectProxy::IsSelected)
      .SetProperty("isSelectable", &WebAXObjectProxy::IsSelectable)
      .SetProperty("isMultiSelectable", &WebAXObjectProxy::IsMultiSelectable)
      .SetProperty("isSelectedOptionActive",
                   &WebAXObjectProxy::IsSelectedOptionActive)
      .SetProperty("isExpanded", &WebAXObjectProxy::IsExpanded)
      .SetProperty("isChecked", &WebAXObjectProxy::IsChecked)
      .SetProperty("isVisible", &WebAXObjectProxy::IsVisible)
      .SetProperty("isOffScreen", &WebAXObjectProxy::IsOffScreen)
      .SetProperty("isCollapsed", &WebAXObjectProxy::IsCollapsed)
      .SetProperty("hasPopup", &WebAXObjectProxy::HasPopup)
      .SetProperty("isValid", &WebAXObjectProxy::IsValid)
      .SetProperty("isReadOnly", &WebAXObjectProxy::IsReadOnly)
      .SetProperty("backgroundColor", &WebAXObjectProxy::BackgroundColor)
      .SetProperty("color", &WebAXObjectProxy::Color)
      .SetProperty("colorValue", &WebAXObjectProxy::ColorValue)
      .SetProperty("fontFamily", &WebAXObjectProxy::FontFamily)
      .SetProperty("fontSize", &WebAXObjectProxy::FontSize)
      .SetProperty("orientation", &WebAXObjectProxy::Orientation)
      .SetProperty("posInSet", &WebAXObjectProxy::PosInSet)
      .SetProperty("setSize", &WebAXObjectProxy::SetSize)
      .SetProperty("clickPointX", &WebAXObjectProxy::ClickPointX)
      .SetProperty("clickPointY", &WebAXObjectProxy::ClickPointY)
      .SetProperty("rowCount", &WebAXObjectProxy::RowCount)
      .SetProperty("rowHeadersCount", &WebAXObjectProxy::RowHeadersCount)
      .SetProperty("columnCount", &WebAXObjectProxy::ColumnCount)
      .SetProperty("columnHeadersCount", &WebAXObjectProxy::ColumnHeadersCount)
      .SetProperty("isClickable", &WebAXObjectProxy::IsClickable)
      .SetProperty("isButtonStateMixed", &WebAXObjectProxy::IsButtonStateMixed)
      //
      // NEW bounding rect calculation - high-level interface
      //
      .SetProperty("boundsX", &WebAXObjectProxy::BoundsX)
      .SetProperty("boundsY", &WebAXObjectProxy::BoundsY)
      .SetProperty("boundsWidth", &WebAXObjectProxy::BoundsWidth)
      .SetProperty("boundsHeight", &WebAXObjectProxy::BoundsHeight)
      .SetMethod("allAttributes", &WebAXObjectProxy::AllAttributes)
      .SetMethod("attributesOfChildren",
                 &WebAXObjectProxy::AttributesOfChildren)
      .SetMethod("ariaControlsElementAtIndex",
                 &WebAXObjectProxy::AriaControlsElementAtIndex)
      .SetMethod("ariaFlowToElementAtIndex",
                 &WebAXObjectProxy::AriaFlowToElementAtIndex)
      .SetMethod("ariaOwnsElementAtIndex",
                 &WebAXObjectProxy::AriaOwnsElementAtIndex)
      .SetMethod("lineForIndex", &WebAXObjectProxy::LineForIndex)
      .SetMethod("boundsForRange", &WebAXObjectProxy::BoundsForRange)
      .SetMethod("childAtIndex", &WebAXObjectProxy::ChildAtIndex)
      .SetMethod("elementAtPoint", &WebAXObjectProxy::ElementAtPoint)
      .SetMethod("tableHeader", &WebAXObjectProxy::TableHeader)
      .SetMethod("rowHeaderAtIndex", &WebAXObjectProxy::RowHeaderAtIndex)
      .SetMethod("columnHeaderAtIndex", &WebAXObjectProxy::ColumnHeaderAtIndex)
      .SetMethod("rowIndexRange", &WebAXObjectProxy::RowIndexRange)
      .SetMethod("columnIndexRange", &WebAXObjectProxy::ColumnIndexRange)
      .SetMethod("cellForColumnAndRow", &WebAXObjectProxy::CellForColumnAndRow)
      .SetMethod("setSelectedTextRange",
                 &WebAXObjectProxy::SetSelectedTextRange)
      .SetMethod("setSelection", &WebAXObjectProxy::SetSelection)
      .SetMethod("isAttributeSettable", &WebAXObjectProxy::IsAttributeSettable)
      .SetMethod("isPressActionSupported",
                 &WebAXObjectProxy::IsPressActionSupported)
      .SetMethod("isIncrementActionSupported",
                 &WebAXObjectProxy::IsIncrementActionSupported)
      .SetMethod("isDecrementActionSupported",
                 &WebAXObjectProxy::IsDecrementActionSupported)
      .SetMethod("parentElement", &WebAXObjectProxy::ParentElement)
      .SetMethod("increment", &WebAXObjectProxy::Increment)
      .SetMethod("decrement", &WebAXObjectProxy::Decrement)
      .SetMethod("showMenu", &WebAXObjectProxy::ShowMenu)
      .SetMethod("press", &WebAXObjectProxy::Press)
      .SetMethod("setValue", &WebAXObjectProxy::SetValue)
      .SetMethod("isEqual", &WebAXObjectProxy::IsEqual)
      .SetMethod("setNotificationListener",
                 &WebAXObjectProxy::SetNotificationListener)
      .SetMethod("unsetNotificationListener",
                 &WebAXObjectProxy::UnsetNotificationListener)
      .SetMethod("takeFocus", &WebAXObjectProxy::TakeFocus)
      .SetMethod("scrollToMakeVisible", &WebAXObjectProxy::ScrollToMakeVisible)
      .SetMethod("scrollToMakeVisibleWithSubFocus",
                 &WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus)
      .SetMethod("scrollToGlobalPoint", &WebAXObjectProxy::ScrollToGlobalPoint)
      .SetMethod("scrollX", &WebAXObjectProxy::ScrollX)
      .SetMethod("scrollY", &WebAXObjectProxy::ScrollY)
      .SetMethod("wordStart", &WebAXObjectProxy::WordStart)
      .SetMethod("wordEnd", &WebAXObjectProxy::WordEnd)
      .SetMethod("nextOnLine", &WebAXObjectProxy::NextOnLine)
      .SetMethod("previousOnLine", &WebAXObjectProxy::PreviousOnLine)
      .SetMethod("misspellingAtIndex", &WebAXObjectProxy::MisspellingAtIndex)
      // TODO(hajimehoshi): This is for backward compatibility. Remove them.
      .SetMethod("addNotificationListener",
                 &WebAXObjectProxy::SetNotificationListener)
      .SetMethod("removeNotificationListener",
                 &WebAXObjectProxy::UnsetNotificationListener)
      //
      // NEW accessible name and description accessors
      //
      .SetProperty("name", &WebAXObjectProxy::Name)
      .SetProperty("nameFrom", &WebAXObjectProxy::NameFrom)
      .SetMethod("nameElementCount", &WebAXObjectProxy::NameElementCount)
      .SetMethod("nameElementAtIndex", &WebAXObjectProxy::NameElementAtIndex)
      .SetProperty("description", &WebAXObjectProxy::Description)
      .SetProperty("descriptionFrom", &WebAXObjectProxy::DescriptionFrom)
      .SetProperty("placeholder", &WebAXObjectProxy::Placeholder)
      .SetProperty("misspellingsCount", &WebAXObjectProxy::MisspellingsCount)
      .SetMethod("descriptionElementCount",
                 &WebAXObjectProxy::DescriptionElementCount)
      .SetMethod("descriptionElementAtIndex",
                 &WebAXObjectProxy::DescriptionElementAtIndex)
      //
      // NEW bounding rect calculation - low-level interface
      //
      .SetMethod("offsetContainer", &WebAXObjectProxy::OffsetContainer)
      .SetMethod("boundsInContainerX", &WebAXObjectProxy::BoundsInContainerX)
      .SetMethod("boundsInContainerY", &WebAXObjectProxy::BoundsInContainerY)
      .SetMethod("boundsInContainerWidth",
                 &WebAXObjectProxy::BoundsInContainerWidth)
      .SetMethod("boundsInContainerHeight",
                 &WebAXObjectProxy::BoundsInContainerHeight)
      .SetMethod("hasNonIdentityTransform",
                 &WebAXObjectProxy::HasNonIdentityTransform);
}

v8::Local<v8::Object> WebAXObjectProxy::GetChildAtIndex(unsigned index) {
  return factory_->GetOrCreate(accessibility_object_.childAt(index));
}

bool WebAXObjectProxy::IsRoot() const {
  return false;
}

bool WebAXObjectProxy::IsEqualToObject(const blink::WebAXObject& other) {
  return accessibility_object_.equals(other);
}

void WebAXObjectProxy::NotificationReceived(
    blink::WebFrame* frame,
    const std::string& notification_name) {
  if (notification_callback_.IsEmpty())
    return;

  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Isolate* isolate = blink::mainThreadIsolate();

  v8::Local<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate, notification_name.data(),
                              v8::String::kNormalString,
                              notification_name.size()),
  };
  frame->callFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, notification_callback_),
      context->Global(), arraysize(argv), argv);
}

void WebAXObjectProxy::Reset() {
  notification_callback_.Reset();
}

std::string WebAXObjectProxy::Role() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetRole(accessibility_object_);
}

std::string WebAXObjectProxy::StringValue() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetStringValue(accessibility_object_);
}

std::string WebAXObjectProxy::Language() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetLanguage(accessibility_object_);
}

int WebAXObjectProxy::X() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).x;
}

int WebAXObjectProxy::Y() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).y;
}

int WebAXObjectProxy::Width() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).width;
}

int WebAXObjectProxy::Height() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).height;
}

int WebAXObjectProxy::IntValue() {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (accessibility_object_.supportsRangeValue())
    return accessibility_object_.valueForRange();
  else if (accessibility_object_.role() == blink::WebAXRoleHeading)
    return accessibility_object_.headingLevel();
  else
    return atoi(accessibility_object_.stringValue().utf8().data());
}

int WebAXObjectProxy::MinValue() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.minValueForRange();
}

int WebAXObjectProxy::MaxValue() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.maxValueForRange();
}

std::string WebAXObjectProxy::ValueDescription() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetValueDescription(accessibility_object_);
}

int WebAXObjectProxy::ChildrenCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  int count = 1;  // Root object always has only one child, the WebView.
  if (!IsRoot())
    count = accessibility_object_.childCount();
  return count;
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionAnchorObject() {
  accessibility_object_.updateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  blink::WebAXTextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  blink::WebAXTextAffinity focusAffinity;
  accessibility_object_.selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (anchorObject.isNull())
    return v8::Null(blink::mainThreadIsolate());

  return factory_->GetOrCreate(anchorObject);
}

int WebAXObjectProxy::SelectionAnchorOffset() {
  accessibility_object_.updateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  blink::WebAXTextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  blink::WebAXTextAffinity focusAffinity;
  accessibility_object_.selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (anchorOffset < 0)
    return -1;

  return anchorOffset;
}

std::string WebAXObjectProxy::SelectionAnchorAffinity() {
  accessibility_object_.updateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  blink::WebAXTextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  blink::WebAXTextAffinity focusAffinity;
  accessibility_object_.selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  return anchorAffinity == blink::WebAXTextAffinityUpstream ? "upstream"
                                                            : "downstream";
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionFocusObject() {
  accessibility_object_.updateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  blink::WebAXTextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  blink::WebAXTextAffinity focusAffinity;
  accessibility_object_.selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (focusObject.isNull())
    return v8::Null(blink::mainThreadIsolate());

  return factory_->GetOrCreate(focusObject);
}

int WebAXObjectProxy::SelectionFocusOffset() {
  accessibility_object_.updateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  blink::WebAXTextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  blink::WebAXTextAffinity focusAffinity;
  accessibility_object_.selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (focusOffset < 0)
    return -1;

  return focusOffset;
}

std::string WebAXObjectProxy::SelectionFocusAffinity() {
  accessibility_object_.updateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  blink::WebAXTextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  blink::WebAXTextAffinity focusAffinity;
  accessibility_object_.selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  return focusAffinity == blink::WebAXTextAffinityUpstream ? "upstream"
                                                           : "downstream";
}

int WebAXObjectProxy::SelectionStart() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.selectionStart();
}

int WebAXObjectProxy::SelectionEnd() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.selectionEnd();
}

int WebAXObjectProxy::SelectionStartLineNumber() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.selectionStartLineNumber();
}

int WebAXObjectProxy::SelectionEndLineNumber() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.selectionEndLineNumber();
}

bool WebAXObjectProxy::IsEnabled() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isEnabled();
}

bool WebAXObjectProxy::IsRequired() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isRequired();
}

bool WebAXObjectProxy::IsEditable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isEditable();
}

bool WebAXObjectProxy::IsRichlyEditable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isRichlyEditable();
}

bool WebAXObjectProxy::IsFocused() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isFocused();
}

bool WebAXObjectProxy::IsFocusable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.canSetFocusAttribute();
}

bool WebAXObjectProxy::IsModal() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isModal();
}

bool WebAXObjectProxy::IsSelected() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isSelected();
}

bool WebAXObjectProxy::IsSelectable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.canSetSelectedAttribute();
}

bool WebAXObjectProxy::IsMultiSelectable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isMultiSelectable();
}

bool WebAXObjectProxy::IsSelectedOptionActive() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isSelectedOptionActive();
}

bool WebAXObjectProxy::IsExpanded() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isExpanded() == blink::WebAXExpandedExpanded;
}

bool WebAXObjectProxy::IsChecked() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isChecked();
}

bool WebAXObjectProxy::IsCollapsed() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isExpanded() == blink::WebAXExpandedCollapsed;
}

bool WebAXObjectProxy::IsVisible() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isVisible();
}

bool WebAXObjectProxy::IsOffScreen() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isOffScreen();
}

bool WebAXObjectProxy::HasPopup() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.ariaHasPopup();
}

bool WebAXObjectProxy::IsValid() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return !accessibility_object_.isDetached();
}

bool WebAXObjectProxy::IsReadOnly() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isReadOnly();
}

unsigned int WebAXObjectProxy::BackgroundColor() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.backgroundColor();
}

unsigned int WebAXObjectProxy::Color() {
  accessibility_object_.updateLayoutAndCheckValidity();
  unsigned int color = accessibility_object_.color();
  // Remove the alpha because it's always 1 and thus not informative.
  return color & 0xFFFFFF;
}

// For input elements of type color.
unsigned int WebAXObjectProxy::ColorValue() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.colorValue();
}

std::string WebAXObjectProxy::FontFamily() {
  accessibility_object_.updateLayoutAndCheckValidity();
  std::string font_family(accessibility_object_.fontFamily().utf8());
  return font_family.insert(0, "AXFontFamily: ");
}

float WebAXObjectProxy::FontSize() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.fontSize();
}

std::string WebAXObjectProxy::Orientation() {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (accessibility_object_.orientation() == blink::WebAXOrientationVertical)
    return "AXOrientation: AXVerticalOrientation";
  else if (accessibility_object_.orientation() ==
           blink::WebAXOrientationHorizontal)
    return "AXOrientation: AXHorizontalOrientation";

  return std::string();
}

int WebAXObjectProxy::PosInSet() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.posInSet();
}

int WebAXObjectProxy::SetSize() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.setSize();
}

int WebAXObjectProxy::ClickPointX() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebFloatRect bounds = BoundsForObject(accessibility_object_);
  return bounds.x + bounds.width / 2;
}

int WebAXObjectProxy::ClickPointY() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebFloatRect bounds = BoundsForObject(accessibility_object_);
  return bounds.y + bounds.height / 2;
}

int32_t WebAXObjectProxy::RowCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return static_cast<int32_t>(accessibility_object_.rowCount());
}

int32_t WebAXObjectProxy::RowHeadersCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.rowHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

int32_t WebAXObjectProxy::ColumnCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return static_cast<int32_t>(accessibility_object_.columnCount());
}

int32_t WebAXObjectProxy::ColumnHeadersCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.columnHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

bool WebAXObjectProxy::IsClickable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isClickable();
}

bool WebAXObjectProxy::IsButtonStateMixed() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isButtonStateMixed();
}

v8::Local<v8::Object> WebAXObjectProxy::AriaControlsElementAtIndex(
    unsigned index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.getSparseAXAttributes(attribute_adapter);
  blink::WebVector<blink::WebAXObject> elements =
      attribute_adapter.object_vector_attributes
          [blink::WebAXObjectVectorAttribute::AriaControls];
  size_t elementCount = elements.size();
  if (index >= elementCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaFlowToElementAtIndex(
    unsigned index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.getSparseAXAttributes(attribute_adapter);
  blink::WebVector<blink::WebAXObject> elements =
      attribute_adapter.object_vector_attributes
          [blink::WebAXObjectVectorAttribute::AriaFlowTo];
  size_t elementCount = elements.size();
  if (index >= elementCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaOwnsElementAtIndex(unsigned index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> elements;
  accessibility_object_.ariaOwns(elements);
  size_t elementCount = elements.size();
  if (index >= elementCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

std::string WebAXObjectProxy::AllAttributes() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetAttributes(accessibility_object_);
}

std::string WebAXObjectProxy::AttributesOfChildren() {
  accessibility_object_.updateLayoutAndCheckValidity();
  AttributesCollector collector;
  unsigned size = accessibility_object_.childCount();
  for (unsigned i = 0; i < size; ++i)
    collector.CollectAttributes(accessibility_object_.childAt(i));
  return collector.attributes();
}

int WebAXObjectProxy::LineForIndex(int index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebVector<int> line_breaks;
  accessibility_object_.lineBreaks(line_breaks);
  int line = 0;
  int vector_size = static_cast<int>(line_breaks.size());
  while (line < vector_size && line_breaks[line] <= index)
    line++;
  return line;
}

std::string WebAXObjectProxy::BoundsForRange(int start, int end) {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (accessibility_object_.role() != blink::WebAXRoleStaticText)
    return std::string();

  if (!accessibility_object_.updateLayoutAndCheckValidity())
    return std::string();

  int len = end - start;

  // Get the bounds for each character and union them into one large rectangle.
  // This is just for testing so it doesn't need to be efficient.
  blink::WebRect bounds = BoundsForCharacter(accessibility_object_, start);
  for (int i = 1; i < len; i++) {
    blink::WebRect next = BoundsForCharacter(accessibility_object_, start + i);
    int right = std::max(bounds.x + bounds.width, next.x + next.width);
    int bottom = std::max(bounds.y + bounds.height, next.y + next.height);
    bounds.x = std::min(bounds.x, next.x);
    bounds.y = std::min(bounds.y, next.y);
    bounds.width = right - bounds.x;
    bounds.height = bottom - bounds.y;
  }

  return base::StringPrintf("{x: %d, y: %d, width: %d, height: %d}", bounds.x,
                            bounds.y, bounds.width, bounds.height);
}

v8::Local<v8::Object> WebAXObjectProxy::ChildAtIndex(int index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetChildAtIndex(index);
}

v8::Local<v8::Object> WebAXObjectProxy::ElementAtPoint(int x, int y) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebPoint point(x, y);
  blink::WebAXObject obj = accessibility_object_.hitTest(point);
  if (obj.isNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::TableHeader() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject obj = accessibility_object_.headerContainerObject();
  if (obj.isNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::RowHeaderAtIndex(unsigned index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.rowHeaders(headers);
  size_t headerCount = headers.size();
  if (index >= headerCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(headers[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::ColumnHeaderAtIndex(unsigned index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.columnHeaders(headers);
  size_t headerCount = headers.size();
  if (index >= headerCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(headers[index]);
}

std::string WebAXObjectProxy::RowIndexRange() {
  accessibility_object_.updateLayoutAndCheckValidity();
  unsigned row_index = accessibility_object_.cellRowIndex();
  unsigned row_span = accessibility_object_.cellRowSpan();
  return base::StringPrintf("{%d, %d}", row_index, row_span);
}

std::string WebAXObjectProxy::ColumnIndexRange() {
  accessibility_object_.updateLayoutAndCheckValidity();
  unsigned column_index = accessibility_object_.cellColumnIndex();
  unsigned column_span = accessibility_object_.cellColumnSpan();
  return base::StringPrintf("{%d, %d}", column_index, column_span);
}

v8::Local<v8::Object> WebAXObjectProxy::CellForColumnAndRow(int column,
                                                            int row) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject obj =
      accessibility_object_.cellForColumnAndRow(column, row);
  if (obj.isNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

void WebAXObjectProxy::SetSelectedTextRange(int selection_start, int length) {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.setSelectedTextRange(selection_start,
                                             selection_start + length);
}

void WebAXObjectProxy::SetSelection(v8::Local<v8::Value> anchor_object,
                                    int anchor_offset,
                                    v8::Local<v8::Value> focus_object,
                                    int focus_offset) {
  if (anchor_object.IsEmpty() || focus_object.IsEmpty() ||
      !anchor_object->IsObject() || !focus_object->IsObject() ||
      anchor_offset < 0 || focus_offset < 0) {
    return;
  }

  WebAXObjectProxy* web_ax_anchor = nullptr;
  if (!gin::ConvertFromV8(blink::mainThreadIsolate(), anchor_object,
                          &web_ax_anchor)) {
    return;
  }
  DCHECK(web_ax_anchor);

  WebAXObjectProxy* web_ax_focus = nullptr;
  if (!gin::ConvertFromV8(blink::mainThreadIsolate(), focus_object,
                          &web_ax_focus)) {
    return;
  }
  DCHECK(web_ax_focus);

  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.setSelection(
      web_ax_anchor->accessibility_object_, anchor_offset,
      web_ax_focus->accessibility_object_, focus_offset);
}

bool WebAXObjectProxy::IsAttributeSettable(const std::string& attribute) {
  accessibility_object_.updateLayoutAndCheckValidity();
  bool settable = false;
  if (attribute == "AXValue")
    settable = accessibility_object_.canSetValueAttribute();
  return settable;
}

bool WebAXObjectProxy::IsPressActionSupported() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.canPress();
}

bool WebAXObjectProxy::IsIncrementActionSupported() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.canIncrement();
}

bool WebAXObjectProxy::IsDecrementActionSupported() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.canDecrement();
}

v8::Local<v8::Object> WebAXObjectProxy::ParentElement() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject parent_object = accessibility_object_.parentObject();
  while (parent_object.accessibilityIsIgnored())
    parent_object = parent_object.parentObject();
  return factory_->GetOrCreate(parent_object);
}

void WebAXObjectProxy::Increment() {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.increment();
}

void WebAXObjectProxy::Decrement() {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.decrement();
}

void WebAXObjectProxy::ShowMenu() {
  accessibility_object_.showContextMenu();
}

void WebAXObjectProxy::Press() {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.press();
}

bool WebAXObjectProxy::SetValue(const std::string& value) {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (!accessibility_object_.canSetValueAttribute())
    return false;

  accessibility_object_.setValue(blink::WebString::fromUTF8(value));
  return true;
}

bool WebAXObjectProxy::IsEqual(v8::Local<v8::Object> proxy) {
  WebAXObjectProxy* unwrapped_proxy = NULL;
  if (!gin::ConvertFromV8(blink::mainThreadIsolate(), proxy, &unwrapped_proxy))
    return false;
  return unwrapped_proxy->IsEqualToObject(accessibility_object_);
}

void WebAXObjectProxy::SetNotificationListener(
    v8::Local<v8::Function> callback) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  notification_callback_.Reset(isolate, callback);
}

void WebAXObjectProxy::UnsetNotificationListener() {
  notification_callback_.Reset();
}

void WebAXObjectProxy::TakeFocus() {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.setFocused(true);
}

void WebAXObjectProxy::ScrollToMakeVisible() {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.scrollToMakeVisible();
}

void WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus(int x,
                                                       int y,
                                                       int width,
                                                       int height) {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.scrollToMakeVisibleWithSubFocus(
      blink::WebRect(x, y, width, height));
}

void WebAXObjectProxy::ScrollToGlobalPoint(int x, int y) {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.scrollToGlobalPoint(blink::WebPoint(x, y));
}

int WebAXObjectProxy::ScrollX() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.getScrollOffset().x;
}

int WebAXObjectProxy::ScrollY() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.getScrollOffset().y;
}

float WebAXObjectProxy::BoundsX() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).x;
}

float WebAXObjectProxy::BoundsY() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).y;
}

float WebAXObjectProxy::BoundsWidth() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).width;
}

float WebAXObjectProxy::BoundsHeight() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).height;
}

int WebAXObjectProxy::WordStart(int character_index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (accessibility_object_.role() != blink::WebAXRoleStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_start;
}

int WebAXObjectProxy::WordEnd(int character_index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (accessibility_object_.role() != blink::WebAXRoleStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_end;
}

v8::Local<v8::Object> WebAXObjectProxy::NextOnLine() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject obj = accessibility_object_.nextOnLine();
  if (obj.isNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::PreviousOnLine() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject obj = accessibility_object_.previousOnLine();
  if (obj.isNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

std::string WebAXObjectProxy::MisspellingAtIndex(int index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (index < 0 || index >= MisspellingsCount())
    return std::string();
  return GetMisspellings(accessibility_object_)[index];
}

std::string WebAXObjectProxy::Name() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.name().utf8();
}

std::string WebAXObjectProxy::NameFrom() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom = blink::WebAXNameFromUninitialized;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  switch (nameFrom) {
    case blink::WebAXNameFromUninitialized:
      return "";
    case blink::WebAXNameFromAttribute:
      return "attribute";
    case blink::WebAXNameFromCaption:
      return "caption";
    case blink::WebAXNameFromContents:
      return "contents";
    case blink::WebAXNameFromPlaceholder:
      return "placeholder";
    case blink::WebAXNameFromRelatedElement:
      return "relatedElement";
    case blink::WebAXNameFromValue:
      return "value";
    case blink::WebAXNameFromTitle:
      return "title";
  }

  NOTREACHED();
  return std::string();
}

int WebAXObjectProxy::NameElementCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  return static_cast<int>(nameObjects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::NameElementAtIndex(unsigned index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  if (index >= nameObjects.size())
    return v8::Local<v8::Object>();
  return factory_->GetOrCreate(nameObjects[index]);
}

std::string WebAXObjectProxy::Description() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  blink::WebAXDescriptionFrom descriptionFrom;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  return accessibility_object_
      .description(nameFrom, descriptionFrom, descriptionObjects)
      .utf8();
}

std::string WebAXObjectProxy::DescriptionFrom() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  blink::WebAXDescriptionFrom descriptionFrom =
      blink::WebAXDescriptionFromUninitialized;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  accessibility_object_.description(nameFrom, descriptionFrom,
                                    descriptionObjects);
  switch (descriptionFrom) {
    case blink::WebAXDescriptionFromUninitialized:
      return "";
    case blink::WebAXDescriptionFromAttribute:
      return "attribute";
    case blink::WebAXDescriptionFromContents:
      return "contents";
    case blink::WebAXDescriptionFromRelatedElement:
      return "relatedElement";
  }

  NOTREACHED();
  return std::string();
}

std::string WebAXObjectProxy::Placeholder() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  return accessibility_object_.placeholder(nameFrom).utf8();
}

int WebAXObjectProxy::MisspellingsCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetMisspellings(accessibility_object_).size();
}

int WebAXObjectProxy::DescriptionElementCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  blink::WebAXDescriptionFrom descriptionFrom;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  accessibility_object_.description(nameFrom, descriptionFrom,
                                    descriptionObjects);
  return static_cast<int>(descriptionObjects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::DescriptionElementAtIndex(
    unsigned index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.name(nameFrom, nameObjects);
  blink::WebAXDescriptionFrom descriptionFrom;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  accessibility_object_.description(nameFrom, descriptionFrom,
                                    descriptionObjects);
  if (index >= descriptionObjects.size())
    return v8::Local<v8::Object>();
  return factory_->GetOrCreate(descriptionObjects[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::OffsetContainer() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.getRelativeBounds(container, bounds, matrix);
  return factory_->GetOrCreate(container);
}

float WebAXObjectProxy::BoundsInContainerX() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.getRelativeBounds(container, bounds, matrix);
  return bounds.x;
}

float WebAXObjectProxy::BoundsInContainerY() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.getRelativeBounds(container, bounds, matrix);
  return bounds.y;
}

float WebAXObjectProxy::BoundsInContainerWidth() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.getRelativeBounds(container, bounds, matrix);
  return bounds.width;
}

float WebAXObjectProxy::BoundsInContainerHeight() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.getRelativeBounds(container, bounds, matrix);
  return bounds.height;
}

bool WebAXObjectProxy::HasNonIdentityTransform() {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.getRelativeBounds(container, bounds, matrix);
  return !matrix.isIdentity();
}

RootWebAXObjectProxy::RootWebAXObjectProxy(const blink::WebAXObject& object,
                                           Factory* factory)
    : WebAXObjectProxy(object, factory) {}

v8::Local<v8::Object> RootWebAXObjectProxy::GetChildAtIndex(unsigned index) {
  if (index)
    return v8::Local<v8::Object>();

  return factory()->GetOrCreate(accessibility_object());
}

bool RootWebAXObjectProxy::IsRoot() const {
  return true;
}

WebAXObjectProxyList::WebAXObjectProxyList()
    : elements_(blink::mainThreadIsolate()) {}

WebAXObjectProxyList::~WebAXObjectProxyList() {
  Clear();
}

void WebAXObjectProxyList::Clear() {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  size_t elementCount = elements_.Size();
  for (size_t i = 0; i < elementCount; i++) {
    WebAXObjectProxy* unwrapped_object = NULL;
    bool result =
        gin::ConvertFromV8(isolate, elements_.Get(i), &unwrapped_object);
    DCHECK(result);
    DCHECK(unwrapped_object);
    unwrapped_object->Reset();
  }
  elements_.Clear();
}

v8::Local<v8::Object> WebAXObjectProxyList::GetOrCreate(
    const blink::WebAXObject& object) {
  if (object.isNull())
    return v8::Local<v8::Object>();

  v8::Isolate* isolate = blink::mainThreadIsolate();

  size_t elementCount = elements_.Size();
  for (size_t i = 0; i < elementCount; i++) {
    WebAXObjectProxy* unwrapped_object = NULL;
    bool result =
        gin::ConvertFromV8(isolate, elements_.Get(i), &unwrapped_object);
    DCHECK(result);
    DCHECK(unwrapped_object);
    if (unwrapped_object->IsEqualToObject(object))
      return elements_.Get(i);
  }

  v8::Local<v8::Value> value_handle =
      gin::CreateHandle(isolate, new WebAXObjectProxy(object, this)).ToV8();
  if (value_handle.IsEmpty())
    return v8::Local<v8::Object>();
  v8::Local<v8::Object> handle = value_handle->ToObject(isolate);
  elements_.Append(handle);
  return handle;
}

}  // namespace test_runner
