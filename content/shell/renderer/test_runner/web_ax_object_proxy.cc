// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/web_ax_object_proxy.h"

#include "base/strings/stringprintf.h"
#include "gin/handle.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
std::string RoleToString(blink::WebAXRole role)
{
  std::string result = "AXRole: AX";
  switch (role) {
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
    case blink::WebAXRoleBanner:
      return result.append("Banner");
    case blink::WebAXRoleBrowser:
      return result.append("Browser");
    case blink::WebAXRoleBusyIndicator:
      return result.append("BusyIndicator");
    case blink::WebAXRoleButton:
      return result.append("Button");
    case blink::WebAXRoleCanvas:
      return result.append("Canvas");
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
    case blink::WebAXRoleDefinition:
      return result.append("Definition");
    case blink::WebAXRoleDescriptionListDetail:
      return result.append("DescriptionListDetail");
    case blink::WebAXRoleDescriptionListTerm:
      return result.append("DescriptionListTerm");
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
    case blink::WebAXRoleDrawer:
      return result.append("Drawer");
    case blink::WebAXRoleEditableText:
      return result.append("EditableText");
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
    case blink::WebAXRoleGrowArea:
      return result.append("GrowArea");
    case blink::WebAXRoleHeading:
      return result.append("Heading");
    case blink::WebAXRoleHelpTag:
      return result.append("HelpTag");
    case blink::WebAXRoleHorizontalRule:
      return result.append("HorizontalRule");
    case blink::WebAXRoleIgnored:
      return result.append("Ignored");
    case blink::WebAXRoleImageMapLink:
      return result.append("ImageMapLink");
    case blink::WebAXRoleImageMap:
      return result.append("ImageMap");
    case blink::WebAXRoleImage:
      return result.append("Image");
    case blink::WebAXRoleIncrementor:
      return result.append("Incrementor");
    case blink::WebAXRoleInlineTextBox:
      return result.append("InlineTextBox");
    case blink::WebAXRoleLabel:
      return result.append("Label");
    case blink::WebAXRoleLegend:
      return result.append("Legend");
    case blink::WebAXRoleLink:
      return result.append("Link");
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
    case blink::WebAXRoleMarquee:
      return result.append("Marquee");
    case blink::WebAXRoleMathElement:
      return result.append("MathElement");
    case blink::WebAXRoleMath:
      return result.append("Math");
    case blink::WebAXRoleMatte:
      return result.append("Matte");
    case blink::WebAXRoleMenuBar:
      return result.append("MenuBar");
    case blink::WebAXRoleMenuButton:
      return result.append("MenuButton");
    case blink::WebAXRoleMenuItem:
      return result.append("MenuItem");
    case blink::WebAXRoleMenuListOption:
      return result.append("MenuListOption");
    case blink::WebAXRoleMenuListPopup:
      return result.append("MenuListPopup");
    case blink::WebAXRoleMenu:
      return result.append("Menu");
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
    case blink::WebAXRoleRulerMarker:
      return result.append("RulerMarker");
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
    case blink::WebAXRoleSheet:
      return result.append("Sheet");
    case blink::WebAXRoleSlider:
      return result.append("Slider");
    case blink::WebAXRoleSliderThumb:
      return result.append("SliderThumb");
    case blink::WebAXRoleSpinButtonPart:
      return result.append("SpinButtonPart");
    case blink::WebAXRoleSpinButton:
      return result.append("SpinButton");
    case blink::WebAXRoleSplitGroup:
      return result.append("SplitGroup");
    case blink::WebAXRoleSplitter:
      return result.append("Splitter");
    case blink::WebAXRoleStaticText:
      return result.append("StaticText");
    case blink::WebAXRoleStatus:
      return result.append("Status");
    case blink::WebAXRoleSystemWide:
      return result.append("SystemWide");
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
    case blink::WebAXRoleTextArea:
      return result.append("TextArea");
    case blink::WebAXRoleTextField:
      return result.append("TextField");
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
    case blink::WebAXRoleValueIndicator:
      return result.append("ValueIndicator");
    case blink::WebAXRoleWebArea:
      return result.append("WebArea");
    case blink::WebAXRoleWindow:
      return result.append("Window");
    default:
      return result.append("Unknown");
  }
}

std::string GetDescription(const blink::WebAXObject& object) {
  std::string description = object.accessibilityDescription().utf8();
  return description.insert(0, "AXDescription: ");
}

std::string GetHelpText(const blink::WebAXObject& object) {
  std::string help_text = object.helpText().utf8();
  return help_text.insert(0, "AXHelp: ");
}

std::string GetStringValue(const blink::WebAXObject& object) {
  std::string value;
  if (object.role() == blink::WebAXRoleColorWell) {
    int r, g, b;
    object.colorValue(r, g, b);
    value = base::StringPrintf("rgb %7.5f %7.5f %7.5f 1",
                               r / 255., g / 255., b / 255.);
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

std::string GetTitle(const blink::WebAXObject& object) {
  std::string title = object.title().utf8();
  return title.insert(0, "AXTitle: ");
}

std::string GetOrientation(const blink::WebAXObject& object) {
  if (object.isVertical())
    return "AXOrientation: AXVerticalOrientation";

  return "AXOrientation: AXHorizontalOrientation";
}

std::string GetValueDescription(const blink::WebAXObject& object) {
  std::string value_description = object.valueDescription().utf8();
  return value_description.insert(0, "AXValueDescription: ");
}

std::string GetAttributes(const blink::WebAXObject& object) {
  // FIXME: Concatenate all attributes of the AXObject.
  std::string attributes(GetTitle(object));
  attributes.append("\n");
  attributes.append(GetRole(object));
  attributes.append("\n");
  attributes.append(GetDescription(object));
  return attributes;
}

blink::WebRect BoundsForCharacter(const blink::WebAXObject& object,
                                  int characterIndex) {
  DCHECK_EQ(object.role(), blink::WebAXRoleStaticText);
  int end = 0;
  for (unsigned i = 0; i < object.childCount(); i++) {
    blink::WebAXObject inline_text_box = object.childAt(i);
    DCHECK_EQ(inline_text_box.role(), blink::WebAXRoleInlineTextBox);
    int start = end;
    end += inline_text_box.stringValue().length();
    if (characterIndex < start || characterIndex >= end)
      continue;
    blink::WebRect inline_text_box_rect = inline_text_box.boundingBoxRect();
    int localIndex = characterIndex - start;
    blink::WebVector<int> character_offsets;
    inline_text_box.characterOffsets(character_offsets);
    DCHECK(character_offsets.size() > 0 &&
           character_offsets.size() == inline_text_box.stringValue().length());
    switch (inline_text_box.textDirection()) {
      case blink::WebAXTextDirectionLR: {
        if (localIndex) {
          int left = inline_text_box_rect.x + character_offsets[localIndex - 1];
          int width = character_offsets[localIndex] -
              character_offsets[localIndex - 1];
          return blink::WebRect(left, inline_text_box_rect.y,
                                width, inline_text_box_rect.height);
        }
        return blink::WebRect(
            inline_text_box_rect.x, inline_text_box_rect.y,
            character_offsets[0], inline_text_box_rect.height);
      }
      case blink::WebAXTextDirectionRL: {
        int right = inline_text_box_rect.x + inline_text_box_rect.width;

        if (localIndex) {
          int left = right - character_offsets[localIndex];
          int width = character_offsets[localIndex] -
              character_offsets[localIndex - 1];
          return blink::WebRect(left, inline_text_box_rect.y,
                                width, inline_text_box_rect.height);
        }
        int left = right - character_offsets[0];
        return blink::WebRect(
            left, inline_text_box_rect.y,
            character_offsets[0], inline_text_box_rect.height);
      }
      case blink::WebAXTextDirectionTB: {
        if (localIndex) {
          int top = inline_text_box_rect.y + character_offsets[localIndex - 1];
          int height = character_offsets[localIndex] -
              character_offsets[localIndex - 1];
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
          int height = character_offsets[localIndex] -
              character_offsets[localIndex - 1];
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

void GetBoundariesForOneWord(const blink::WebAXObject& object,
                             int character_index,
                             int& word_start,
                             int& word_end) {
  int end = 0;
  for (unsigned i = 0; i < object.childCount(); i++) {
    blink::WebAXObject inline_text_box = object.childAt(i);
    DCHECK_EQ(inline_text_box.role(), blink::WebAXRoleInlineTextBox);
    int start = end;
    end += inline_text_box.stringValue().length();
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

}  // namespace

gin::WrapperInfo WebAXObjectProxy::kWrapperInfo = {
    gin::kEmbedderNativeGin};

WebAXObjectProxy::WebAXObjectProxy(const blink::WebAXObject& object,
                                   WebAXObjectProxy::Factory* factory)
    : accessibility_object_(object),
      factory_(factory) {
}

WebAXObjectProxy::~WebAXObjectProxy() {}

gin::ObjectTemplateBuilder
WebAXObjectProxy::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<WebAXObjectProxy>::GetObjectTemplateBuilder(isolate)
      .SetProperty("role", &WebAXObjectProxy::Role)
      .SetProperty("title", &WebAXObjectProxy::Title)
      .SetProperty("description", &WebAXObjectProxy::Description)
      .SetProperty("helpText", &WebAXObjectProxy::HelpText)
      .SetProperty("stringValue", &WebAXObjectProxy::StringValue)
      .SetProperty("x", &WebAXObjectProxy::X)
      .SetProperty("y", &WebAXObjectProxy::Y)
      .SetProperty("width", &WebAXObjectProxy::Width)
      .SetProperty("height", &WebAXObjectProxy::Height)
      .SetProperty("intValue", &WebAXObjectProxy::IntValue)
      .SetProperty("minValue", &WebAXObjectProxy::MinValue)
      .SetProperty("maxValue", &WebAXObjectProxy::MaxValue)
      .SetProperty("valueDescription", &WebAXObjectProxy::ValueDescription)
      .SetProperty("childrenCount", &WebAXObjectProxy::ChildrenCount)
      .SetProperty("insertionPointLineNumber",
                   &WebAXObjectProxy::InsertionPointLineNumber)
      .SetProperty("selectedTextRange", &WebAXObjectProxy::SelectedTextRange)
      .SetProperty("isEnabled", &WebAXObjectProxy::IsEnabled)
      .SetProperty("isRequired", &WebAXObjectProxy::IsRequired)
      .SetProperty("isFocused", &WebAXObjectProxy::IsFocused)
      .SetProperty("isFocusable", &WebAXObjectProxy::IsFocusable)
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
      .SetProperty("orientation", &WebAXObjectProxy::Orientation)
      .SetProperty("clickPointX", &WebAXObjectProxy::ClickPointX)
      .SetProperty("clickPointY", &WebAXObjectProxy::ClickPointY)
      .SetProperty("rowCount", &WebAXObjectProxy::RowCount)
      .SetProperty("columnCount", &WebAXObjectProxy::ColumnCount)
      .SetProperty("isClickable", &WebAXObjectProxy::IsClickable)
      .SetMethod("allAttributes", &WebAXObjectProxy::AllAttributes)
      .SetMethod("attributesOfChildren",
                 &WebAXObjectProxy::AttributesOfChildren)
      .SetMethod("lineForIndex", &WebAXObjectProxy::LineForIndex)
      .SetMethod("boundsForRange", &WebAXObjectProxy::BoundsForRange)
      .SetMethod("childAtIndex", &WebAXObjectProxy::ChildAtIndex)
      .SetMethod("elementAtPoint", &WebAXObjectProxy::ElementAtPoint)
      .SetMethod("tableHeader", &WebAXObjectProxy::TableHeader)
      .SetMethod("rowIndexRange", &WebAXObjectProxy::RowIndexRange)
      .SetMethod("columnIndexRange", &WebAXObjectProxy::ColumnIndexRange)
      .SetMethod("cellForColumnAndRow", &WebAXObjectProxy::CellForColumnAndRow)
      .SetMethod("titleUIElement", &WebAXObjectProxy::TitleUIElement)
      .SetMethod("setSelectedTextRange",
                 &WebAXObjectProxy::SetSelectedTextRange)
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
      .SetMethod("wordStart", &WebAXObjectProxy::WordStart)
      .SetMethod("wordEnd", &WebAXObjectProxy::WordEnd)
      // TODO(hajimehoshi): This is for backward compatibility. Remove them.
      .SetMethod("addNotificationListener",
                 &WebAXObjectProxy::SetNotificationListener)
      .SetMethod("removeNotificationListener",
                 &WebAXObjectProxy::UnsetNotificationListener);
}

v8::Handle<v8::Object> WebAXObjectProxy::GetChildAtIndex(unsigned index) {
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

  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Isolate* isolate = blink::mainThreadIsolate();

  v8::Handle<v8::Value> argv[] = {
    v8::String::NewFromUtf8(isolate, notification_name.data(),
                            v8::String::kNormalString,
                            notification_name.size()),
  };
  frame->callFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, notification_callback_),
      context->Global(),
      arraysize(argv),
      argv);
}

void WebAXObjectProxy::Reset()  {
  notification_callback_.Reset();
}

std::string WebAXObjectProxy::Role() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetRole(accessibility_object_);
}

std::string WebAXObjectProxy::Title() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetTitle(accessibility_object_);
}

std::string WebAXObjectProxy::Description() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetDescription(accessibility_object_);
}

std::string WebAXObjectProxy::HelpText() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetHelpText(accessibility_object_);
}

std::string WebAXObjectProxy::StringValue() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetStringValue(accessibility_object_);
}

int WebAXObjectProxy::X() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.boundingBoxRect().x;
}

int WebAXObjectProxy::Y() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.boundingBoxRect().y;
}

int WebAXObjectProxy::Width() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.boundingBoxRect().width;
}

int WebAXObjectProxy::Height() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.boundingBoxRect().height;
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
  int count = 1; // Root object always has only one child, the WebView.
  if (!IsRoot())
    count = accessibility_object_.childCount();
  return count;
}

int WebAXObjectProxy::InsertionPointLineNumber() {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (!accessibility_object_.isFocused())
    return -1;
  return accessibility_object_.selectionEndLineNumber();
}

std::string WebAXObjectProxy::SelectedTextRange() {
  accessibility_object_.updateLayoutAndCheckValidity();
  unsigned selection_start = accessibility_object_.selectionStart();
  unsigned selection_end = accessibility_object_.selectionEnd();
  return base::StringPrintf("{%d, %d}",
                            selection_start, selection_end - selection_start);
}

bool WebAXObjectProxy::IsEnabled() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isEnabled();
}

bool WebAXObjectProxy::IsRequired() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isRequired();
}

bool WebAXObjectProxy::IsFocused() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isFocused();
}

bool WebAXObjectProxy::IsFocusable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.canSetFocusAttribute();
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
  return !accessibility_object_.isCollapsed();
}

bool WebAXObjectProxy::IsChecked() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isChecked();
}

bool WebAXObjectProxy::IsVisible() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isVisible();
}

bool WebAXObjectProxy::IsOffScreen() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isOffScreen();
}

bool WebAXObjectProxy::IsCollapsed() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isCollapsed();
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

std::string WebAXObjectProxy::Orientation() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetOrientation(accessibility_object_);
}

int WebAXObjectProxy::ClickPointX() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.clickPoint().x;
}

int WebAXObjectProxy::ClickPointY() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.clickPoint().y;
}

int32_t WebAXObjectProxy::RowCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return static_cast<int32_t>(accessibility_object_.rowCount());
}

int32_t WebAXObjectProxy::ColumnCount() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return static_cast<int32_t>(accessibility_object_.columnCount());
}

bool WebAXObjectProxy::IsClickable() {
  accessibility_object_.updateLayoutAndCheckValidity();
  return accessibility_object_.isClickable();
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

  return base::StringPrintf("{x: %d, y: %d, width: %d, height: %d}",
                            bounds.x, bounds.y, bounds.width, bounds.height);
}

v8::Handle<v8::Object> WebAXObjectProxy::ChildAtIndex(int index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  return GetChildAtIndex(index);
}

v8::Handle<v8::Object> WebAXObjectProxy::ElementAtPoint(int x, int y) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebPoint point(x, y);
  blink::WebAXObject obj = accessibility_object_.hitTest(point);
  if (obj.isNull())
    return v8::Handle<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Handle<v8::Object> WebAXObjectProxy::TableHeader() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject obj = accessibility_object_.headerContainerObject();
  if (obj.isNull())
    return v8::Handle<v8::Object>();

  return factory_->GetOrCreate(obj);
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

v8::Handle<v8::Object> WebAXObjectProxy::CellForColumnAndRow(
    int column, int row) {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject obj =
      accessibility_object_.cellForColumnAndRow(column, row);
  if (obj.isNull())
    return v8::Handle<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Handle<v8::Object> WebAXObjectProxy::TitleUIElement() {
  accessibility_object_.updateLayoutAndCheckValidity();
  blink::WebAXObject obj = accessibility_object_.titleUIElement();
  if (obj.isNull())
    return v8::Handle<v8::Object>();

  return factory_->GetOrCreate(obj);
}

void WebAXObjectProxy::SetSelectedTextRange(int selection_start,
                                            int length) {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.setSelectedTextRange(selection_start,
                                              selection_start + length);
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

v8::Handle<v8::Object> WebAXObjectProxy::ParentElement() {
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
}

void WebAXObjectProxy::Press() {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.press();
}

bool WebAXObjectProxy::IsEqual(v8::Handle<v8::Object> proxy) {
  WebAXObjectProxy* unwrapped_proxy = NULL;
  if (!gin::ConvertFromV8(blink::mainThreadIsolate(), proxy, &unwrapped_proxy))
    return false;
  return unwrapped_proxy->IsEqualToObject(accessibility_object_);
}

void WebAXObjectProxy::SetNotificationListener(
    v8::Handle<v8::Function> callback) {
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

void WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus(int x, int y,
                                                       int width, int height) {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.scrollToMakeVisibleWithSubFocus(
      blink::WebRect(x, y, width, height));
}

void WebAXObjectProxy::ScrollToGlobalPoint(int x, int y) {
  accessibility_object_.updateLayoutAndCheckValidity();
  accessibility_object_.scrollToGlobalPoint(blink::WebPoint(x, y));
}

int WebAXObjectProxy::WordStart(int character_index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (accessibility_object_.role() != blink::WebAXRoleStaticText)
    return -1;

  int word_start, word_end;
  GetBoundariesForOneWord(accessibility_object_, character_index,
                          word_start, word_end);
  return word_start;
}

int WebAXObjectProxy::WordEnd(int character_index) {
  accessibility_object_.updateLayoutAndCheckValidity();
  if (accessibility_object_.role() != blink::WebAXRoleStaticText)
    return -1;

  int word_start, word_end;
  GetBoundariesForOneWord(accessibility_object_, character_index,
                          word_start, word_end);
  return word_end;
}

RootWebAXObjectProxy::RootWebAXObjectProxy(
    const blink::WebAXObject &object, Factory *factory)
    : WebAXObjectProxy(object, factory) {
}

v8::Handle<v8::Object> RootWebAXObjectProxy::GetChildAtIndex(unsigned index) {
  if (index)
    return v8::Handle<v8::Object>();

  return factory()->GetOrCreate(accessibility_object());
}

bool RootWebAXObjectProxy::IsRoot() const {
  return true;
}

WebAXObjectProxyList::WebAXObjectProxyList()
    : elements_(blink::mainThreadIsolate()) {
}

WebAXObjectProxyList::~WebAXObjectProxyList() {
  Clear();
}

void WebAXObjectProxyList::Clear() {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  size_t elementCount = elements_.Size();
  for (size_t i = 0; i < elementCount; i++) {
    WebAXObjectProxy* unwrapped_object = NULL;
    bool result = gin::ConvertFromV8(isolate, elements_.Get(i),
                                     &unwrapped_object);
    DCHECK(result);
    DCHECK(unwrapped_object);
    unwrapped_object->Reset();
  }
  elements_.Clear();
}

v8::Handle<v8::Object> WebAXObjectProxyList::GetOrCreate(
    const blink::WebAXObject& object) {
  if (object.isNull())
    return v8::Handle<v8::Object>();

  v8::Isolate* isolate = blink::mainThreadIsolate();

  size_t elementCount = elements_.Size();
  for (size_t i = 0; i < elementCount; i++) {
    WebAXObjectProxy* unwrapped_object = NULL;
    bool result = gin::ConvertFromV8(isolate, elements_.Get(i),
                                     &unwrapped_object);
    DCHECK(result);
    DCHECK(unwrapped_object);
    if (unwrapped_object->IsEqualToObject(object))
      return elements_.Get(i);
  }

  v8::Handle<v8::Value> value_handle = gin::CreateHandle(
      isolate, new WebAXObjectProxy(object, this)).ToV8();
  if (value_handle.IsEmpty())
    return v8::Handle<v8::Object>();
  v8::Handle<v8::Object> handle = value_handle->ToObject();
  elements_.Append(handle);
  return handle;
}

v8::Handle<v8::Object> WebAXObjectProxyList::CreateRoot(
    const blink::WebAXObject& object) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::Handle<v8::Value> value_handle = gin::CreateHandle(
      isolate, new RootWebAXObjectProxy(object, this)).ToV8();
  if (value_handle.IsEmpty())
    return v8::Handle<v8::Object>();
  v8::Handle<v8::Object> handle = value_handle->ToObject();
  elements_.Append(handle);
  return handle;
}

}  // namespace content
