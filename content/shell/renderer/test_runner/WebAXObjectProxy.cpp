// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/WebAXObjectProxy.h"

#include "content/shell/renderer/test_runner/TestCommon.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"

using namespace blink;
using namespace std;

namespace WebTestRunner {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
string roleToString(WebAXRole role)
{
    string result = "AXRole: AX";
    switch (role) {
    case WebAXRoleAlertDialog:
        return result.append("AlertDialog");
    case WebAXRoleAlert:
        return result.append("Alert");
    case WebAXRoleAnnotation:
        return result.append("Annotation");
    case WebAXRoleApplication:
        return result.append("Application");
    case WebAXRoleArticle:
        return result.append("Article");
    case WebAXRoleBanner:
        return result.append("Banner");
    case WebAXRoleBrowser:
        return result.append("Browser");
    case WebAXRoleBusyIndicator:
        return result.append("BusyIndicator");
    case WebAXRoleButton:
        return result.append("Button");
    case WebAXRoleCanvas:
        return result.append("Canvas");
    case WebAXRoleCell:
        return result.append("Cell");
    case WebAXRoleCheckBox:
        return result.append("CheckBox");
    case WebAXRoleColorWell:
        return result.append("ColorWell");
    case WebAXRoleColumnHeader:
        return result.append("ColumnHeader");
    case WebAXRoleColumn:
        return result.append("Column");
    case WebAXRoleComboBox:
        return result.append("ComboBox");
    case WebAXRoleComplementary:
        return result.append("Complementary");
    case WebAXRoleContentInfo:
        return result.append("ContentInfo");
    case WebAXRoleDefinition:
        return result.append("Definition");
    case WebAXRoleDescriptionListDetail:
        return result.append("DescriptionListDetail");
    case WebAXRoleDescriptionListTerm:
        return result.append("DescriptionListTerm");
    case WebAXRoleDialog:
        return result.append("Dialog");
    case WebAXRoleDirectory:
        return result.append("Directory");
    case WebAXRoleDisclosureTriangle:
        return result.append("DisclosureTriangle");
    case WebAXRoleDiv:
        return result.append("Div");
    case WebAXRoleDocument:
        return result.append("Document");
    case WebAXRoleDrawer:
        return result.append("Drawer");
    case WebAXRoleEditableText:
        return result.append("EditableText");
    case WebAXRoleFooter:
        return result.append("Footer");
    case WebAXRoleForm:
        return result.append("Form");
    case WebAXRoleGrid:
        return result.append("Grid");
    case WebAXRoleGroup:
        return result.append("Group");
    case WebAXRoleGrowArea:
        return result.append("GrowArea");
    case WebAXRoleHeading:
        return result.append("Heading");
    case WebAXRoleHelpTag:
        return result.append("HelpTag");
    case WebAXRoleHorizontalRule:
        return result.append("HorizontalRule");
    case WebAXRoleIgnored:
        return result.append("Ignored");
    case WebAXRoleImageMapLink:
        return result.append("ImageMapLink");
    case WebAXRoleImageMap:
        return result.append("ImageMap");
    case WebAXRoleImage:
        return result.append("Image");
    case WebAXRoleIncrementor:
        return result.append("Incrementor");
    case WebAXRoleInlineTextBox:
        return result.append("InlineTextBox");
    case WebAXRoleLabel:
        return result.append("Label");
    case WebAXRoleLegend:
        return result.append("Legend");
    case WebAXRoleLink:
        return result.append("Link");
    case WebAXRoleListBoxOption:
        return result.append("ListBoxOption");
    case WebAXRoleListBox:
        return result.append("ListBox");
    case WebAXRoleListItem:
        return result.append("ListItem");
    case WebAXRoleListMarker:
        return result.append("ListMarker");
    case WebAXRoleList:
        return result.append("List");
    case WebAXRoleLog:
        return result.append("Log");
    case WebAXRoleMain:
        return result.append("Main");
    case WebAXRoleMarquee:
        return result.append("Marquee");
    case WebAXRoleMathElement:
        return result.append("MathElement");
    case WebAXRoleMath:
        return result.append("Math");
    case WebAXRoleMatte:
        return result.append("Matte");
    case WebAXRoleMenuBar:
        return result.append("MenuBar");
    case WebAXRoleMenuButton:
        return result.append("MenuButton");
    case WebAXRoleMenuItem:
        return result.append("MenuItem");
    case WebAXRoleMenuListOption:
        return result.append("MenuListOption");
    case WebAXRoleMenuListPopup:
        return result.append("MenuListPopup");
    case WebAXRoleMenu:
        return result.append("Menu");
    case WebAXRoleNavigation:
        return result.append("Navigation");
    case WebAXRoleNote:
        return result.append("Note");
    case WebAXRoleOutline:
        return result.append("Outline");
    case WebAXRoleParagraph:
        return result.append("Paragraph");
    case WebAXRolePopUpButton:
        return result.append("PopUpButton");
    case WebAXRolePresentational:
        return result.append("Presentational");
    case WebAXRoleProgressIndicator:
        return result.append("ProgressIndicator");
    case WebAXRoleRadioButton:
        return result.append("RadioButton");
    case WebAXRoleRadioGroup:
        return result.append("RadioGroup");
    case WebAXRoleRegion:
        return result.append("Region");
    case WebAXRoleRootWebArea:
        return result.append("RootWebArea");
    case WebAXRoleRowHeader:
        return result.append("RowHeader");
    case WebAXRoleRow:
        return result.append("Row");
    case WebAXRoleRulerMarker:
        return result.append("RulerMarker");
    case WebAXRoleRuler:
        return result.append("Ruler");
    case WebAXRoleSVGRoot:
        return result.append("SVGRoot");
    case WebAXRoleScrollArea:
        return result.append("ScrollArea");
    case WebAXRoleScrollBar:
        return result.append("ScrollBar");
    case WebAXRoleSeamlessWebArea:
        return result.append("SeamlessWebArea");
    case WebAXRoleSearch:
        return result.append("Search");
    case WebAXRoleSheet:
        return result.append("Sheet");
    case WebAXRoleSlider:
        return result.append("Slider");
    case WebAXRoleSliderThumb:
        return result.append("SliderThumb");
    case WebAXRoleSpinButtonPart:
        return result.append("SpinButtonPart");
    case WebAXRoleSpinButton:
        return result.append("SpinButton");
    case WebAXRoleSplitGroup:
        return result.append("SplitGroup");
    case WebAXRoleSplitter:
        return result.append("Splitter");
    case WebAXRoleStaticText:
        return result.append("StaticText");
    case WebAXRoleStatus:
        return result.append("Status");
    case WebAXRoleSystemWide:
        return result.append("SystemWide");
    case WebAXRoleTabGroup:
        return result.append("TabGroup");
    case WebAXRoleTabList:
        return result.append("TabList");
    case WebAXRoleTabPanel:
        return result.append("TabPanel");
    case WebAXRoleTab:
        return result.append("Tab");
    case WebAXRoleTableHeaderContainer:
        return result.append("TableHeaderContainer");
    case WebAXRoleTable:
        return result.append("Table");
    case WebAXRoleTextArea:
        return result.append("TextArea");
    case WebAXRoleTextField:
        return result.append("TextField");
    case WebAXRoleTimer:
        return result.append("Timer");
    case WebAXRoleToggleButton:
        return result.append("ToggleButton");
    case WebAXRoleToolbar:
        return result.append("Toolbar");
    case WebAXRoleTreeGrid:
        return result.append("TreeGrid");
    case WebAXRoleTreeItem:
        return result.append("TreeItem");
    case WebAXRoleTree:
        return result.append("Tree");
    case WebAXRoleUnknown:
        return result.append("Unknown");
    case WebAXRoleUserInterfaceTooltip:
        return result.append("UserInterfaceTooltip");
    case WebAXRoleValueIndicator:
        return result.append("ValueIndicator");
    case WebAXRoleWebArea:
        return result.append("WebArea");
    case WebAXRoleWindow:
        return result.append("Window");
    default:
        return result.append("Unknown");
    }
}

string getDescription(const WebAXObject& object)
{
    string description = object.accessibilityDescription().utf8();
    return description.insert(0, "AXDescription: ");
}

string getHelpText(const WebAXObject& object)
{
    string helpText = object.helpText().utf8();
    return helpText.insert(0, "AXHelp: ");
}

string getStringValue(const WebAXObject& object)
{
    string value;
    if (object.role() == WebAXRoleColorWell) {
        int r, g, b;
        char buffer[100];
        object.colorValue(r, g, b);
        snprintf(buffer, sizeof(buffer), "rgb %7.5f %7.5f %7.5f 1", r / 255., g / 255., b / 255.);
        value = buffer;
    } else {
        value = object.stringValue().utf8();
    }
    return value.insert(0, "AXValue: ");
}

string getRole(const WebAXObject& object)
{
    string roleString = roleToString(object.role());

    // Special-case canvas with fallback content because Chromium wants to
    // treat this as essentially a separate role that it can map differently depending
    // on the platform.
    if (object.role() == WebAXRoleCanvas && object.canvasHasFallbackContent())
        roleString += "WithFallbackContent";

    return roleString;
}

string getTitle(const WebAXObject& object)
{
    string title = object.title().utf8();
    return title.insert(0, "AXTitle: ");
}

string getOrientation(const WebAXObject& object)
{
    if (object.isVertical())
        return "AXOrientation: AXVerticalOrientation";

    return "AXOrientation: AXHorizontalOrientation";
}

string getValueDescription(const WebAXObject& object)
{
    string valueDescription = object.valueDescription().utf8();
    return valueDescription.insert(0, "AXValueDescription: ");
}

string getAttributes(const WebAXObject& object)
{
    // FIXME: Concatenate all attributes of the AXObject.
    string attributes(getTitle(object));
    attributes.append("\n");
    attributes.append(getRole(object));
    attributes.append("\n");
    attributes.append(getDescription(object));
    return attributes;
}

WebRect boundsForCharacter(const WebAXObject& object, int characterIndex)
{
    BLINK_ASSERT(object.role() == WebAXRoleStaticText);
    int end = 0;
    for (unsigned i = 0; i < object.childCount(); i++) {
        WebAXObject inlineTextBox = object.childAt(i);
        BLINK_ASSERT(inlineTextBox.role() == WebAXRoleInlineTextBox);
        int start = end;
        end += inlineTextBox.stringValue().length();
        if (end <= characterIndex)
            continue;
        WebRect inlineTextBoxRect = inlineTextBox.boundingBoxRect();
        int localIndex = characterIndex - start;
        WebVector<int> characterOffsets;
        inlineTextBox.characterOffsets(characterOffsets);
        BLINK_ASSERT(characterOffsets.size() > 0 && characterOffsets.size() == inlineTextBox.stringValue().length());
        switch (inlineTextBox.textDirection()) {
        case WebAXTextDirectionLR: {
            if (localIndex) {
                int left = inlineTextBoxRect.x + characterOffsets[localIndex - 1];
                int width = characterOffsets[localIndex] - characterOffsets[localIndex - 1];
                return WebRect(left, inlineTextBoxRect.y, width, inlineTextBoxRect.height);
            }
            return WebRect(inlineTextBoxRect.x, inlineTextBoxRect.y, characterOffsets[0], inlineTextBoxRect.height);
        }
        case WebAXTextDirectionRL: {
            int right = inlineTextBoxRect.x + inlineTextBoxRect.width;

            if (localIndex) {
                int left = right - characterOffsets[localIndex];
                int width = characterOffsets[localIndex] - characterOffsets[localIndex - 1];
                return WebRect(left, inlineTextBoxRect.y, width, inlineTextBoxRect.height);
            }
            int left = right - characterOffsets[0];
            return WebRect(left, inlineTextBoxRect.y, characterOffsets[0], inlineTextBoxRect.height);
        }
        case WebAXTextDirectionTB: {
            if (localIndex) {
                int top = inlineTextBoxRect.y + characterOffsets[localIndex - 1];
                int height = characterOffsets[localIndex] - characterOffsets[localIndex - 1];
                return WebRect(inlineTextBoxRect.x, top, inlineTextBoxRect.width, height);
            }
            return WebRect(inlineTextBoxRect.x, inlineTextBoxRect.y, inlineTextBoxRect.width, characterOffsets[0]);
        }
        case WebAXTextDirectionBT: {
            int bottom = inlineTextBoxRect.y + inlineTextBoxRect.height;

            if (localIndex) {
                int top = bottom - characterOffsets[localIndex];
                int height = characterOffsets[localIndex] - characterOffsets[localIndex - 1];
                return WebRect(inlineTextBoxRect.x, top, inlineTextBoxRect.width, height);
            }
            int top = bottom - characterOffsets[0];
            return WebRect(inlineTextBoxRect.x, top, inlineTextBoxRect.width, characterOffsets[0]);
        }
        }
    }

    BLINK_ASSERT(false);
    return WebRect();
}

void getBoundariesForOneWord(const WebAXObject& object, int characterIndex, int& wordStart, int& wordEnd)
{
    int end = 0;
    for (unsigned i = 0; i < object.childCount(); i++) {
        WebAXObject inlineTextBox = object.childAt(i);
        BLINK_ASSERT(inlineTextBox.role() == WebAXRoleInlineTextBox);
        int start = end;
        end += inlineTextBox.stringValue().length();
        if (end <= characterIndex)
            continue;
        int localIndex = characterIndex - start;

        WebVector<int> starts;
        WebVector<int> ends;
        inlineTextBox.wordBoundaries(starts, ends);
        size_t wordCount = starts.size();
        BLINK_ASSERT(ends.size() == wordCount);

        // If there are no words, use the InlineTextBox boundaries.
        if (!wordCount) {
            wordStart = start;
            wordEnd = end;
            return;
        }

        // Look for a character within any word other than the last.
        for (size_t j = 0; j < wordCount - 1; j++) {
            if (localIndex <= ends[j]) {
                wordStart = start + starts[j];
                wordEnd = start + ends[j];
                return;
            }
        }

        // Return the last word by default.
        wordStart = start + starts[wordCount - 1];
        wordEnd = start + ends[wordCount - 1];
        return;
    }
}

// Collects attributes into a string, delimited by dashes. Used by all methods
// that output lists of attributes: attributesOfLinkedUIElementsCallback,
// AttributesOfChildrenCallback, etc.
class AttributesCollector {
public:
    void collectAttributes(const WebAXObject& object)
    {
        m_attributes.append("\n------------\n");
        m_attributes.append(getAttributes(object));
    }

    string attributes() const { return m_attributes; }

private:
    string m_attributes;
};

}

WebAXObjectProxy::WebAXObjectProxy(const WebAXObject& object, Factory* factory)
    : m_accessibilityObject(object)
    , m_factory(factory)
{

    BLINK_ASSERT(factory);

    //
    // Properties
    //

    bindProperty("role", &WebAXObjectProxy::roleGetterCallback);
    bindProperty("title", &WebAXObjectProxy::titleGetterCallback);
    bindProperty("description", &WebAXObjectProxy::descriptionGetterCallback);
    bindProperty("helpText", &WebAXObjectProxy::helpTextGetterCallback);
    bindProperty("stringValue", &WebAXObjectProxy::stringValueGetterCallback);
    bindProperty("x", &WebAXObjectProxy::xGetterCallback);
    bindProperty("y", &WebAXObjectProxy::yGetterCallback);
    bindProperty("width", &WebAXObjectProxy::widthGetterCallback);
    bindProperty("height", &WebAXObjectProxy::heightGetterCallback);
    bindProperty("intValue", &WebAXObjectProxy::intValueGetterCallback);
    bindProperty("minValue", &WebAXObjectProxy::minValueGetterCallback);
    bindProperty("maxValue", &WebAXObjectProxy::maxValueGetterCallback);
    bindProperty("valueDescription", &WebAXObjectProxy::valueDescriptionGetterCallback);
    bindProperty("childrenCount", &WebAXObjectProxy::childrenCountGetterCallback);
    bindProperty("insertionPointLineNumber", &WebAXObjectProxy::insertionPointLineNumberGetterCallback);
    bindProperty("selectedTextRange", &WebAXObjectProxy::selectedTextRangeGetterCallback);
    bindProperty("isEnabled", &WebAXObjectProxy::isEnabledGetterCallback);
    bindProperty("isRequired", &WebAXObjectProxy::isRequiredGetterCallback);
    bindProperty("isFocused", &WebAXObjectProxy::isFocusedGetterCallback);
    bindProperty("isFocusable", &WebAXObjectProxy::isFocusableGetterCallback);
    bindProperty("isSelected", &WebAXObjectProxy::isSelectedGetterCallback);
    bindProperty("isSelectable", &WebAXObjectProxy::isSelectableGetterCallback);
    bindProperty("isMultiSelectable", &WebAXObjectProxy::isMultiSelectableGetterCallback);
    bindProperty("isSelectedOptionActive", &WebAXObjectProxy::isSelectedOptionActiveGetterCallback);
    bindProperty("isExpanded", &WebAXObjectProxy::isExpandedGetterCallback);
    bindProperty("isChecked", &WebAXObjectProxy::isCheckedGetterCallback);
    bindProperty("isVisible", &WebAXObjectProxy::isVisibleGetterCallback);
    bindProperty("isOffScreen", &WebAXObjectProxy::isOffScreenGetterCallback);
    bindProperty("isCollapsed", &WebAXObjectProxy::isCollapsedGetterCallback);
    bindProperty("hasPopup", &WebAXObjectProxy::hasPopupGetterCallback);
    bindProperty("isValid", &WebAXObjectProxy::isValidGetterCallback);
    bindProperty("isReadOnly", &WebAXObjectProxy::isReadOnlyGetterCallback);
    bindProperty("orientation", &WebAXObjectProxy::orientationGetterCallback);
    bindProperty("clickPointX", &WebAXObjectProxy::clickPointXGetterCallback);
    bindProperty("clickPointY", &WebAXObjectProxy::clickPointYGetterCallback);
    bindProperty("rowCount", &WebAXObjectProxy::rowCountGetterCallback);
    bindProperty("columnCount", &WebAXObjectProxy::columnCountGetterCallback);
    bindProperty("isClickable", &WebAXObjectProxy::isClickableGetterCallback);

    //
    // Methods
    //

    bindMethod("allAttributes", &WebAXObjectProxy::allAttributesCallback);
    bindMethod("attributesOfChildren", &WebAXObjectProxy::attributesOfChildrenCallback);
    bindMethod("lineForIndex", &WebAXObjectProxy::lineForIndexCallback);
    bindMethod("boundsForRange", &WebAXObjectProxy::boundsForRangeCallback);
    bindMethod("childAtIndex", &WebAXObjectProxy::childAtIndexCallback);
    bindMethod("elementAtPoint", &WebAXObjectProxy::elementAtPointCallback);
    bindMethod("tableHeader", &WebAXObjectProxy::tableHeaderCallback);
    bindMethod("rowIndexRange", &WebAXObjectProxy::rowIndexRangeCallback);
    bindMethod("columnIndexRange", &WebAXObjectProxy::columnIndexRangeCallback);
    bindMethod("cellForColumnAndRow", &WebAXObjectProxy::cellForColumnAndRowCallback);
    bindMethod("titleUIElement", &WebAXObjectProxy::titleUIElementCallback);
    bindMethod("setSelectedTextRange", &WebAXObjectProxy::setSelectedTextRangeCallback);
    bindMethod("isAttributeSettable", &WebAXObjectProxy::isAttributeSettableCallback);
    bindMethod("isPressActionSupported", &WebAXObjectProxy::isPressActionSupportedCallback);
    bindMethod("isIncrementActionSupported", &WebAXObjectProxy::isIncrementActionSupportedCallback);
    bindMethod("isDecrementActionSupported", &WebAXObjectProxy::isDecrementActionSupportedCallback);
    bindMethod("parentElement", &WebAXObjectProxy::parentElementCallback);
    bindMethod("increment", &WebAXObjectProxy::incrementCallback);
    bindMethod("decrement", &WebAXObjectProxy::decrementCallback);
    bindMethod("showMenu", &WebAXObjectProxy::showMenuCallback);
    bindMethod("press", &WebAXObjectProxy::pressCallback);
    bindMethod("isEqual", &WebAXObjectProxy::isEqualCallback);
    bindMethod("addNotificationListener", &WebAXObjectProxy::addNotificationListenerCallback);
    bindMethod("removeNotificationListener", &WebAXObjectProxy::removeNotificationListenerCallback);
    bindMethod("takeFocus", &WebAXObjectProxy::takeFocusCallback);
    bindMethod("scrollToMakeVisible", &WebAXObjectProxy::scrollToMakeVisibleCallback);
    bindMethod("scrollToMakeVisibleWithSubFocus", &WebAXObjectProxy::scrollToMakeVisibleWithSubFocusCallback);
    bindMethod("scrollToGlobalPoint", &WebAXObjectProxy::scrollToGlobalPointCallback);
    bindMethod("wordStart", &WebAXObjectProxy::wordStartCallback);
    bindMethod("wordEnd", &WebAXObjectProxy::wordEndCallback);

    bindFallbackMethod(&WebAXObjectProxy::fallbackCallback);
}

WebAXObjectProxy::~WebAXObjectProxy()
{
}

WebAXObjectProxy* WebAXObjectProxy::getChildAtIndex(unsigned index)
{
    return m_factory->getOrCreate(accessibilityObject().childAt(index));
}

bool WebAXObjectProxy::isRoot() const
{
    return false;
}

bool WebAXObjectProxy::isEqual(const blink::WebAXObject& other)
{
    return accessibilityObject().equals(other);
}

void WebAXObjectProxy::notificationReceived(const char* notificationName)
{
    size_t callbackCount = m_notificationCallbacks.size();
    for (size_t i = 0; i < callbackCount; i++) {
        CppVariant notificationNameArgument;
        notificationNameArgument.set(notificationName);
        CppVariant invokeResult;
        m_notificationCallbacks[i].invokeDefault(&notificationNameArgument, 1, invokeResult);
    }
}

//
// Properties
//

void WebAXObjectProxy::roleGetterCallback(CppVariant* result)
{
    result->set(getRole(accessibilityObject()));
}

void WebAXObjectProxy::titleGetterCallback(CppVariant* result)
{
    result->set(getTitle(accessibilityObject()));
}

void WebAXObjectProxy::descriptionGetterCallback(CppVariant* result)
{
    result->set(getDescription(accessibilityObject()));
}

void WebAXObjectProxy::helpTextGetterCallback(CppVariant* result)
{
    result->set(getHelpText(accessibilityObject()));
}

void WebAXObjectProxy::stringValueGetterCallback(CppVariant* result)
{
    result->set(getStringValue(accessibilityObject()));
}

void WebAXObjectProxy::xGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().x);
}

void WebAXObjectProxy::yGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().y);
}

void WebAXObjectProxy::widthGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().width);
}

void WebAXObjectProxy::heightGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().height);
}

void WebAXObjectProxy::intValueGetterCallback(CppVariant* result)
{
    if (accessibilityObject().supportsRangeValue())
        result->set(accessibilityObject().valueForRange());
    else if (accessibilityObject().role() == WebAXRoleHeading)
        result->set(accessibilityObject().headingLevel());
    else
        result->set(atoi(accessibilityObject().stringValue().utf8().data()));
}

void WebAXObjectProxy::minValueGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().minValueForRange());
}

void WebAXObjectProxy::maxValueGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().maxValueForRange());
}

void WebAXObjectProxy::valueDescriptionGetterCallback(CppVariant* result)
{
    result->set(getValueDescription(accessibilityObject()));
}

void WebAXObjectProxy::childrenCountGetterCallback(CppVariant* result)
{
    int count = 1; // Root object always has only one child, the WebView.
    if (!isRoot())
        count = accessibilityObject().childCount();
    result->set(count);
}

void WebAXObjectProxy::insertionPointLineNumberGetterCallback(CppVariant* result)
{
    if (!accessibilityObject().isFocused()) {
        result->set(-1);
        return;
    }

    int lineNumber = accessibilityObject().selectionEndLineNumber();
    result->set(lineNumber);
}

void WebAXObjectProxy::selectedTextRangeGetterCallback(CppVariant* result)
{
    unsigned selectionStart = accessibilityObject().selectionStart();
    unsigned selectionEnd = accessibilityObject().selectionEnd();
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{%d, %d}", selectionStart, selectionEnd - selectionStart);

    result->set(std::string(buffer));
}

void WebAXObjectProxy::isEnabledGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isEnabled());
}

void WebAXObjectProxy::isRequiredGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isRequired());
}

void WebAXObjectProxy::isFocusedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isFocused());
}

void WebAXObjectProxy::isFocusableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().canSetFocusAttribute());
}

void WebAXObjectProxy::isSelectedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isSelected());
}

void WebAXObjectProxy::isSelectableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().canSetSelectedAttribute());
}

void WebAXObjectProxy::isMultiSelectableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isMultiSelectable());
}

void WebAXObjectProxy::isSelectedOptionActiveGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isSelectedOptionActive());
}

void WebAXObjectProxy::isExpandedGetterCallback(CppVariant* result)
{
    result->set(!accessibilityObject().isCollapsed());
}

void WebAXObjectProxy::isCheckedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isChecked());
}

void WebAXObjectProxy::isVisibleGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isVisible());
}

void WebAXObjectProxy::isOffScreenGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isOffScreen());
}

void WebAXObjectProxy::isCollapsedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isCollapsed());
}

void WebAXObjectProxy::hasPopupGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().ariaHasPopup());
}

void WebAXObjectProxy::isValidGetterCallback(CppVariant* result)
{
    result->set(!accessibilityObject().isDetached());
}

void WebAXObjectProxy::isReadOnlyGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isReadOnly());
}

void WebAXObjectProxy::orientationGetterCallback(CppVariant* result)
{
    result->set(getOrientation(accessibilityObject()));
}

void WebAXObjectProxy::clickPointXGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().clickPoint().x);
}

void WebAXObjectProxy::clickPointYGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().clickPoint().y);
}

void WebAXObjectProxy::rowCountGetterCallback(CppVariant* result)
{
    result->set(static_cast<int32_t>(accessibilityObject().rowCount()));
}

void WebAXObjectProxy::columnCountGetterCallback(CppVariant* result)
{
    result->set(static_cast<int32_t>(accessibilityObject().columnCount()));
}

void WebAXObjectProxy::isClickableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isClickable());
}

//
// Methods
//

void WebAXObjectProxy::allAttributesCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(getAttributes(accessibilityObject()));
}

void WebAXObjectProxy::attributesOfChildrenCallback(const CppArgumentList& arguments, CppVariant* result)
{
    AttributesCollector collector;
    unsigned size = accessibilityObject().childCount();
    for (unsigned i = 0; i < size; ++i)
        collector.collectAttributes(accessibilityObject().childAt(i));
    result->set(collector.attributes());
}

void WebAXObjectProxy::lineForIndexCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (!arguments.size() || !arguments[0].isNumber()) {
        result->setNull();
        return;
    }

    int index = arguments[0].toInt32();

    WebVector<int> lineBreaks;
    accessibilityObject().lineBreaks(lineBreaks);
    int line = 0;
    int vectorSize = static_cast<int>(lineBreaks.size());
    while (line < vectorSize && lineBreaks[line] <= index)
        line++;
    result->set(line);
}

void WebAXObjectProxy::boundsForRangeCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    if (accessibilityObject().role() != WebAXRoleStaticText)
        return;

    int start = arguments[0].toInt32();
    int end = arguments[1].toInt32();
    int len = end - start;

    // Get the bounds for each character and union them into one large rectangle.
    // This is just for testing so it doesn't need to be efficient.
    WebRect bounds = boundsForCharacter(accessibilityObject(), start);
    for (int i = 1; i < len; i++) {
        WebRect next = boundsForCharacter(accessibilityObject(), start + i);
        int right = std::max(bounds.x + bounds.width, next.x + next.width);
        int bottom = std::max(bounds.y + bounds.height, next.y + next.height);
        bounds.x = std::min(bounds.x, next.x);
        bounds.y = std::min(bounds.y, next.y);
        bounds.width = right - bounds.x;
        bounds.height = bottom - bounds.y;
    }

    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{x: %d, y: %d, width: %d, height: %d}", bounds.x, bounds.y, bounds.width, bounds.height);
    result->set(string(buffer));
}

void WebAXObjectProxy::childAtIndexCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (!arguments.size() || !arguments[0].isNumber()) {
        result->setNull();
        return;
    }

    WebAXObjectProxy* child = getChildAtIndex(arguments[0].toInt32());
    if (!child) {
        result->setNull();
        return;
    }

    result->set(*(child->getAsCppVariant()));
}

void WebAXObjectProxy::elementAtPointCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();
    WebPoint point(x, y);
    WebAXObject obj = accessibilityObject().hitTest(point);
    if (obj.isNull())
        return;

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void WebAXObjectProxy::tableHeaderCallback(const CppArgumentList&, CppVariant* result)
{
    WebAXObject obj = accessibilityObject().headerContainerObject();
    if (obj.isNull()) {
        result->setNull();
        return;
    }

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void WebAXObjectProxy::rowIndexRangeCallback(const CppArgumentList&, CppVariant* result)
{
    unsigned rowIndex = accessibilityObject().cellRowIndex();
    unsigned rowSpan = accessibilityObject().cellRowSpan();
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{%d, %d}", rowIndex, rowSpan);
    string value = buffer;
    result->set(std::string(buffer));
}

void WebAXObjectProxy::columnIndexRangeCallback(const CppArgumentList&, CppVariant* result)
{
    unsigned columnIndex = accessibilityObject().cellColumnIndex();
    unsigned columnSpan = accessibilityObject().cellColumnSpan();
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{%d, %d}", columnIndex, columnSpan);
    result->set(std::string(buffer));
}

void WebAXObjectProxy::cellForColumnAndRowCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int column = arguments[0].toInt32();
    int row = arguments[1].toInt32();
    WebAXObject obj = accessibilityObject().cellForColumnAndRow(column, row);
    if (obj.isNull()) {
        result->setNull();
        return;
    }

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void WebAXObjectProxy::titleUIElementCallback(const CppArgumentList&, CppVariant* result)
{
    WebAXObject obj = accessibilityObject().titleUIElement();
    if (obj.isNull()) {
        result->setNull();
        return;
    }

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void WebAXObjectProxy::setSelectedTextRangeCallback(const CppArgumentList&arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int selectionStart = arguments[0].toInt32();
    int selectionEnd = selectionStart + arguments[1].toInt32();
    accessibilityObject().setSelectedTextRange(selectionStart, selectionEnd);
}

void WebAXObjectProxy::isAttributeSettableCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 && !arguments[0].isString()) {
        result->setNull();
        return;
    }

    string attribute = arguments[0].toString();
    bool settable = false;
    if (attribute == "AXValue")
        settable = accessibilityObject().canSetValueAttribute();
    result->set(settable);
}

void WebAXObjectProxy::isPressActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canPress());
}

void WebAXObjectProxy::isIncrementActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canIncrement());
}

void WebAXObjectProxy::isDecrementActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canDecrement());
}

void WebAXObjectProxy::parentElementCallback(const CppArgumentList&, CppVariant* result)
{
    WebAXObject parentObject = accessibilityObject().parentObject();
    while (parentObject.accessibilityIsIgnored())
        parentObject = parentObject.parentObject();
    WebAXObjectProxy* parent = m_factory->getOrCreate(parentObject);
    if (!parent) {
        result->setNull();
        return;
    }

    result->set(*(parent->getAsCppVariant()));
}

void WebAXObjectProxy::incrementCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().increment();
    result->setNull();
}

void WebAXObjectProxy::decrementCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().decrement();
    result->setNull();
}

void WebAXObjectProxy::showMenuCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void WebAXObjectProxy::pressCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().press();
    result->setNull();
}

void WebAXObjectProxy::isEqualCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    result->set(arguments[0].isEqual(*getAsCppVariant()));
}

void WebAXObjectProxy::addNotificationListenerCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    m_notificationCallbacks.push_back(arguments[0]);
    result->setNull();
}

void WebAXObjectProxy::removeNotificationListenerCallback(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void WebAXObjectProxy::takeFocusCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().setFocused(true);
    result->setNull();
}

void WebAXObjectProxy::scrollToMakeVisibleCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().scrollToMakeVisible();
    result->setNull();
}

void WebAXObjectProxy::scrollToMakeVisibleWithSubFocusCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 4
        || !arguments[0].isNumber()
        || !arguments[1].isNumber()
        || !arguments[2].isNumber()
        || !arguments[3].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();
    int width = arguments[2].toInt32();
    int height = arguments[3].toInt32();
    accessibilityObject().scrollToMakeVisibleWithSubFocus(WebRect(x, y, width, height));
    result->setNull();
}

void WebAXObjectProxy::scrollToGlobalPointCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2
        || !arguments[0].isNumber()
        || !arguments[1].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();

    accessibilityObject().scrollToGlobalPoint(WebPoint(x, y));
    result->setNull();
}

void WebAXObjectProxy::wordStartCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 1 || !arguments[0].isNumber())
        return;

    if (accessibilityObject().role() != WebAXRoleStaticText)
        return;

    int characterIndex = arguments[0].toInt32();
    int wordStart, wordEnd;
    getBoundariesForOneWord(accessibilityObject(), characterIndex, wordStart, wordEnd);
    result->set(wordStart);
}

void WebAXObjectProxy::wordEndCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 1 || !arguments[0].isNumber())
        return;

    if (accessibilityObject().role() != WebAXRoleStaticText)
        return;

    int characterIndex = arguments[0].toInt32();
    int wordStart, wordEnd;
    getBoundariesForOneWord(accessibilityObject(), characterIndex, wordStart, wordEnd);
    result->set(wordEnd);
}

void WebAXObjectProxy::fallbackCallback(const CppArgumentList &, CppVariant* result)
{
    result->setNull();
}

RootWebAXObjectProxy::RootWebAXObjectProxy(const WebAXObject &object, Factory *factory)
    : WebAXObjectProxy(object, factory) { }

WebAXObjectProxy* RootWebAXObjectProxy::getChildAtIndex(unsigned index)
{
    if (index)
        return 0;

    return factory()->getOrCreate(accessibilityObject());
}

bool RootWebAXObjectProxy::isRoot() const
{
    return true;
}

WebAXObjectProxyList::WebAXObjectProxyList()
{
}

WebAXObjectProxyList::~WebAXObjectProxyList()
{
    clear();
}

void WebAXObjectProxyList::clear()
{
    for (ElementList::iterator i = m_elements.begin(); i != m_elements.end(); ++i)
        delete (*i);
    m_elements.clear();
}

WebAXObjectProxy* WebAXObjectProxyList::getOrCreate(const WebAXObject& object)
{
    if (object.isNull())
        return 0;

    size_t elementCount = m_elements.size();
    for (size_t i = 0; i < elementCount; i++) {
        if (m_elements[i]->isEqual(object))
            return m_elements[i];
    }

    WebAXObjectProxy* element = new WebAXObjectProxy(object, this);
    m_elements.push_back(element);
    return element;
}

WebAXObjectProxy* WebAXObjectProxyList::createRoot(const WebAXObject& object)
{
    WebAXObjectProxy* element = new RootWebAXObjectProxy(object, this);
    m_elements.push_back(element);
    return element;
}

}
