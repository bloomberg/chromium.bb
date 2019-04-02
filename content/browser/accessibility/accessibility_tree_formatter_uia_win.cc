// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_uia_win.h"

#include <math.h>
#include <oleacc.h>
#include <stddef.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/gfx/win/hwnd_util.h"

namespace {

base::string16 UiaIdentifierToCondensedString16(int32_t id) {
  base::string16 identifier = content::UiaIdentifierToString(id);
  if (id >= UIA_RuntimeIdPropertyId && id <= UIA_HeadingLevelPropertyId) {
    // remove leading 'UIA_' and trailing 'PropertyId'
    return identifier.substr(4, identifier.size() - 14);
  }
  return identifier;
}

std::string UiaIdentifierToCondensedString(int32_t id) {
  return base::UTF16ToUTF8(UiaIdentifierToCondensedString16(id));
}

}  // namespace

namespace content {

// This is the list of interesting properties to dump.
//
// Certain properties are skipped because they are known to cause crashes if the
// underlying pattern isn't implemented (e.g., LegacyIAccessibleSelection will
// crash on Win7 if the LegacyIAccessible pattern isn't implemented).
//
// Other properties aren't interesting in a tree-dump context (e.g., ProcessId).
//
// Finally, certain properties are dumped as part of a pattern, and don't need
// to be dumped a second time here (e.g., Grid*, GridItem*, RangeValue*, etc.).

// static
const long AccessibilityTreeFormatterUia::properties_[] = {
    // UIA_RuntimeIdPropertyId                          // 30000
    UIA_BoundingRectanglePropertyId,                    // 30001
    // UIA_ProcessIdPropertyId                          // 30002
    UIA_ControlTypePropertyId,                          // 30003
    UIA_LocalizedControlTypePropertyId,                 // 30004
    UIA_NamePropertyId,                                 // 30005
    UIA_AcceleratorKeyPropertyId,                       // 30006
    UIA_AccessKeyPropertyId,                            // 30007
    UIA_HasKeyboardFocusPropertyId,                     // 30008
    UIA_IsKeyboardFocusablePropertyId,                  // 30009
    UIA_IsEnabledPropertyId,                            // 30010
    UIA_AutomationIdPropertyId,                         // 30011
    UIA_ClassNamePropertyId,                            // 30012
    UIA_HelpTextPropertyId,                             // 30013
    UIA_ClickablePointPropertyId,                       // 30014
    UIA_CulturePropertyId,                              // 30015
    UIA_IsControlElementPropertyId,                     // 30016
    UIA_IsContentElementPropertyId,                     // 30017
    UIA_LabeledByPropertyId,                            // 30018
    UIA_IsPasswordPropertyId,                           // 30019
    // UIA_NativeWindowHandlePropertyId                 // 30020
    UIA_ItemTypePropertyId,                             // 30021
    UIA_IsOffscreenPropertyId,                          // 30022
    UIA_OrientationPropertyId,                          // 30023
    UIA_FrameworkIdPropertyId,                          // 30024
    UIA_IsRequiredForFormPropertyId,                    // 30025
    UIA_ItemStatusPropertyId,                           // 30026
    UIA_IsDockPatternAvailablePropertyId,               // 30027
    UIA_IsExpandCollapsePatternAvailablePropertyId,     // 30028
    UIA_IsGridItemPatternAvailablePropertyId,           // 30029
    UIA_IsGridPatternAvailablePropertyId,               // 30030
    UIA_IsInvokePatternAvailablePropertyId,             // 30031
    UIA_IsMultipleViewPatternAvailablePropertyId,       // 30032
    UIA_IsRangeValuePatternAvailablePropertyId,         // 30033
    UIA_IsScrollPatternAvailablePropertyId,             // 30034
    UIA_IsScrollItemPatternAvailablePropertyId,         // 30035
    UIA_IsSelectionItemPatternAvailablePropertyId,      // 30036
    UIA_IsSelectionPatternAvailablePropertyId,          // 30037
    UIA_IsTablePatternAvailablePropertyId,              // 30038
    UIA_IsTableItemPatternAvailablePropertyId,          // 30039
    UIA_IsTextPatternAvailablePropertyId,               // 30040
    UIA_IsTogglePatternAvailablePropertyId,             // 30041
    UIA_IsTransformPatternAvailablePropertyId,          // 30042
    UIA_IsValuePatternAvailablePropertyId,              // 30043
    UIA_IsWindowPatternAvailablePropertyId,             // 30044
    // UIA_Value*                                       // 30045-30046
    // UIA_RangeValue*                                  // 30047-30052
    // UIA_Scroll*                                      // 30053-30058
    UIA_SelectionSelectionPropertyId,                   // 30059
    // UIA_Selection*                                   // 30060-30061
    // UIA_Grid*                                        // 30062-30068
    UIA_DockDockPositionPropertyId,                     // 30069
    UIA_ExpandCollapseExpandCollapseStatePropertyId,    // 30070
    UIA_MultipleViewCurrentViewPropertyId,              // 30071
    UIA_MultipleViewSupportedViewsPropertyId,           // 30072
    UIA_WindowCanMaximizePropertyId,                    // 30073
    UIA_WindowCanMinimizePropertyId,                    // 30074
    UIA_WindowWindowVisualStatePropertyId,              // 30075
    UIA_WindowWindowInteractionStatePropertyId,         // 30076
    // UIA_WindowIsModalPropertyId                      // 30077
    UIA_WindowIsTopmostPropertyId,                      // 30078
    // UIA_SelectionItem*                               // 30079-30080
    UIA_TableRowHeadersPropertyId,                      // 30081
    UIA_TableColumnHeadersPropertyId,                   // 30082
    // UIA_TableRowOrColumnMajorPropertyId              // 30083
    UIA_TableItemRowHeaderItemsPropertyId,              // 30084
    UIA_TableItemColumnHeaderItemsPropertyId,           // 30085
    // UIA_ToggleToggleStatePropertyId                  // 30086
    UIA_TransformCanMovePropertyId,                     // 30087
    UIA_TransformCanResizePropertyId,                   // 30088
    UIA_TransformCanRotatePropertyId,                   // 30089
    UIA_IsLegacyIAccessiblePatternAvailablePropertyId,  // 30090
    // UIA_LegacyIAccessible*                           // 30091-30100
    UIA_AriaRolePropertyId,                             // 30101
    UIA_AriaPropertiesPropertyId,                       // 30102
    UIA_IsDataValidForFormPropertyId,                   // 30103
    UIA_ControllerForPropertyId,                        // 30104
    UIA_DescribedByPropertyId,                          // 30105
    UIA_FlowsToPropertyId,                              // 30106
    // UIA_ProviderDescriptionPropertyId                // 30107
    UIA_IsItemContainerPatternAvailablePropertyId,      // 30108
    UIA_IsVirtualizedItemPatternAvailablePropertyId,    // 30109
    UIA_IsSynchronizedInputPatternAvailablePropertyId,  // 30110
    UIA_OptimizeForVisualContentPropertyId,             // 30111
    UIA_IsObjectModelPatternAvailablePropertyId,        // 30112
    UIA_AnnotationAnnotationTypeIdPropertyId,           // 30113
    UIA_AnnotationAnnotationTypeNamePropertyId,         // 30114
    UIA_AnnotationAuthorPropertyId,                     // 30115
    UIA_AnnotationDateTimePropertyId,                   // 30116
    UIA_AnnotationTargetPropertyId,                     // 30117
    UIA_IsAnnotationPatternAvailablePropertyId,         // 30118
    UIA_IsTextPattern2AvailablePropertyId,              // 30119
    UIA_StylesStyleIdPropertyId,                        // 30120
    UIA_StylesStyleNamePropertyId,                      // 30121
    UIA_StylesFillColorPropertyId,                      // 30122
    UIA_StylesFillPatternStylePropertyId,               // 30123
    UIA_StylesShapePropertyId,                          // 30124
    UIA_StylesFillPatternColorPropertyId,               // 30125
    UIA_StylesExtendedPropertiesPropertyId,             // 30126
    UIA_IsStylesPatternAvailablePropertyId,             // 30127
    UIA_IsSpreadsheetPatternAvailablePropertyId,        // 30128
    UIA_SpreadsheetItemFormulaPropertyId,               // 30129
    UIA_SpreadsheetItemAnnotationObjectsPropertyId,     // 30130
    UIA_SpreadsheetItemAnnotationTypesPropertyId,       // 30131
    UIA_IsSpreadsheetItemPatternAvailablePropertyId,    // 30132
    UIA_Transform2CanZoomPropertyId,                    // 30133
    UIA_IsTransformPattern2AvailablePropertyId,         // 30134
    UIA_LiveSettingPropertyId,                          // 30135
    UIA_IsTextChildPatternAvailablePropertyId,          // 30136
    UIA_IsDragPatternAvailablePropertyId,               // 30137
    UIA_DragIsGrabbedPropertyId,                        // 30138
    UIA_DragDropEffectPropertyId,                       // 30139
    UIA_DragDropEffectsPropertyId,                      // 30140
    UIA_IsDropTargetPatternAvailablePropertyId,         // 30141
    UIA_DropTargetDropTargetEffectPropertyId,           // 30142
    UIA_DropTargetDropTargetEffectsPropertyId,          // 30143
    UIA_DragGrabbedItemsPropertyId,                     // 30144
    UIA_Transform2ZoomLevelPropertyId,                  // 30145
    UIA_Transform2ZoomMinimumPropertyId,                // 30146
    UIA_Transform2ZoomMaximumPropertyId,                // 30147
    UIA_FlowsFromPropertyId,                            // 30148
    UIA_IsTextEditPatternAvailablePropertyId,           // 30149
    UIA_IsPeripheralPropertyId,                         // 30150
    UIA_IsCustomNavigationPatternAvailablePropertyId,   // 30151
    UIA_PositionInSetPropertyId,                        // 30152
    UIA_SizeOfSetPropertyId,                            // 30153
    UIA_LevelPropertyId,                                // 30154
    UIA_AnnotationTypesPropertyId,                      // 30155
    UIA_AnnotationObjectsPropertyId,                    // 30156
    UIA_LandmarkTypePropertyId,                         // 30157
    UIA_LocalizedLandmarkTypePropertyId,                // 30158
    UIA_FullDescriptionPropertyId,                      // 30159
    UIA_FillColorPropertyId,                            // 30160
    UIA_OutlineColorPropertyId,                         // 30161
    UIA_FillTypePropertyId,                             // 30162
    UIA_VisualEffectsPropertyId,                        // 30163
    UIA_OutlineThicknessPropertyId,                     // 30164
    UIA_CenterPointPropertyId,                          // 30165
    UIA_RotationPropertyId,                             // 30166
    UIA_SizePropertyId,                                 // 30167
    UIA_IsSelectionPattern2AvailablePropertyId,         // 30168
    UIA_Selection2FirstSelectedItemPropertyId,          // 30169
    UIA_Selection2LastSelectedItemPropertyId,           // 30170
    UIA_Selection2CurrentSelectedItemPropertyId,        // 30171
    UIA_Selection2ItemCountPropertyId,                  // 30172
    UIA_HeadingLevelPropertyId,                         // 30173
};

// static
std::unique_ptr<AccessibilityTreeFormatter>
AccessibilityTreeFormatterUia::CreateUia() {
  base::win::AssertComInitialized();
  return std::make_unique<AccessibilityTreeFormatterUia>();
}

AccessibilityTreeFormatterUia::AccessibilityTreeFormatterUia() {
  // Create an instance of the CUIAutomation class.
  CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                   IID_IUIAutomation, &uia_);
  CHECK(uia_.Get());
  BuildCacheRequests();
}

AccessibilityTreeFormatterUia::~AccessibilityTreeFormatterUia() {}

void AccessibilityTreeFormatterUia::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {
  AddPropertyFilter(property_filters, "Name=*");
  AddPropertyFilter(property_filters, "ItemStatus=*");
  AddPropertyFilter(property_filters, "IsKeyboardFocusable=true");
  AddPropertyFilter(property_filters, "Orientation=OrientationType_Horizontal");
  AddPropertyFilter(property_filters, "IsPassword=true");
  AddPropertyFilter(property_filters, "IsControlElement=false");
  AddPropertyFilter(property_filters, "IsEnabled=false");
  AddPropertyFilter(property_filters, "IsDataValidForForm=true");
  AddPropertyFilter(property_filters, "IsRequiredForForm=true");
}

// static
void AccessibilityTreeFormatterUia::SetUpCommandLineForTestPass(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(::switches::kEnableExperimentalUIAutomation);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterUia::BuildAccessibilityTree(
    BrowserAccessibility* start) {
  // Find the root IUIAutomationElement for the content window.
  HWND hwnd =
      start->manager()->GetRoot()->GetTargetForNativeAccessibilityEvent();
  return BuildAccessibilityTreeForWindow(hwnd);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterUia::BuildAccessibilityTreeForProcess(
    base::ProcessId pid) {
  std::unique_ptr<base::DictionaryValue> tree;
  // Get HWND for process id.
  HWND hwnd = GetHwndForProcess(pid);
  return BuildAccessibilityTreeForWindow(hwnd);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterUia::BuildAccessibilityTreeForWindow(
    gfx::AcceleratedWidget hwnd) {
  CHECK(hwnd);

  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  uia_->ElementFromHandle(hwnd, &root);
  CHECK(root.Get());

  std::unique_ptr<base::DictionaryValue> tree =
      std::make_unique<base::DictionaryValue>();
  RecursiveBuildAccessibilityTree(root.Get(), tree.get());
  return tree;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterUia::BuildAccessibilityTreeForPattern(
    const base::StringPiece& pattern) {
  LOG(ERROR) << "Windows does not yet support building accessibility trees for "
                "patterns";
  return nullptr;
}

void AccessibilityTreeFormatterUia::RecursiveBuildAccessibilityTree(
    IUIAutomationElement* uncached_node,
    base::DictionaryValue* dict) {
  // Process this node.
  AddProperties(uncached_node, dict);

  // Update the cache to get children
  Microsoft::WRL::ComPtr<IUIAutomationElement> parent;
  uncached_node->BuildUpdatedCache(children_cache_request_.Get(), &parent);

  Microsoft::WRL::ComPtr<IUIAutomationElementArray> children;
  if (!SUCCEEDED(parent->GetCachedChildren(&children)) || !children)
    return;
  // Process children.
  auto child_list = std::make_unique<base::ListValue>();
  int child_count;
  children->get_Length(&child_count);
  for (int i = 0; i < child_count; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> child;
    std::unique_ptr<base::DictionaryValue> child_dict =
        std::make_unique<base::DictionaryValue>();
    if (SUCCEEDED(children->GetElement(i, &child))) {
      RecursiveBuildAccessibilityTree(child.Get(), child_dict.get());
    } else {
      child_dict->SetString("error", L"[Error retrieving child]");
    }
    child_list->Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(child_list));
}

void AccessibilityTreeFormatterUia::AddProperties(
    IUIAutomationElement* uncached_node,
    base::DictionaryValue* dict) {
  // Update the cache for this node's information.
  Microsoft::WRL::ComPtr<IUIAutomationElement> node;
  uncached_node->BuildUpdatedCache(element_cache_request_.Get(), &node);

  // Get all properties that may be on this node.
  for (long i : properties_) {
    base::win::ScopedVariant variant;
    if (SUCCEEDED(node->GetCachedPropertyValue(i, variant.Receive()))) {
      WriteProperty(i, variant, dict);
    }
  }
}

void AccessibilityTreeFormatterUia::WriteProperty(
    long propertyId,
    const base::win::ScopedVariant& var,
    base::DictionaryValue* dict) {
  switch (var.type()) {
    case VT_EMPTY:
    case VT_NULL:
      break;
    case VT_I2:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->iVal);
      break;
    case VT_I4:
      WriteI4Property(propertyId, var.ptr()->lVal, dict);
      break;
    case VT_R4:
      dict->SetDouble(UiaIdentifierToCondensedString(propertyId),
                      var.ptr()->fltVal);
      break;
    case VT_R8:
      dict->SetDouble(UiaIdentifierToCondensedString(propertyId),
                      var.ptr()->dblVal);
      break;
    case VT_I1:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->cVal);
      break;
    case VT_UI1:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->bVal);
      break;
    case VT_UI2:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->uiVal);
      break;
    case VT_UI4:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->ulVal);
      break;
      break;
    case VT_BSTR:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      BstrToUTF8(var.ptr()->bstrVal));
      break;
    case VT_BOOL:
      dict->SetBoolean(UiaIdentifierToCondensedString(propertyId),
                       var.ptr()->boolVal == VARIANT_TRUE ? true : false);
      break;
    case VT_UNKNOWN:
      WriteUnknownProperty(propertyId, var.ptr()->punkVal, dict);
      break;
    case VT_DISPATCH:
    case VT_ERROR:
    case VT_CY:
    case VT_DATE:
    case VT_VARIANT:
    case VT_DECIMAL:
    case VT_INT:
    case VT_UINT:
    case VT_ARRAY:
    case VT_BYREF:
    default:
      break;
  }
}

void AccessibilityTreeFormatterUia::WriteI4Property(
    long propertyId,
    long lval,
    base::DictionaryValue* dict) {
  switch (propertyId) {
    case UIA_ControlTypePropertyId:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      UiaIdentifierToCondensedString16(lval));
      break;
    case UIA_OrientationPropertyId:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      UiaOrientationToString(lval));
      break;
    case UIA_LiveSettingPropertyId:
      dict->SetString(UiaIdentifierToCondensedString(propertyId),
                      UiaLiveSettingToString(lval));
      break;
    default:
      dict->SetInteger(UiaIdentifierToCondensedString(propertyId), lval);
      break;
  }
}

void AccessibilityTreeFormatterUia::WriteUnknownProperty(
    long propertyId,
    IUnknown* unk,
    base::DictionaryValue* dict) {
  switch (propertyId) {
    case UIA_ControllerForPropertyId:
    case UIA_DescribedByPropertyId:
    case UIA_FlowsFromPropertyId:
    case UIA_FlowsToPropertyId: {
      Microsoft::WRL::ComPtr<IUIAutomationElementArray> array;
      if (unk && SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&array))))
        WriteElementArray(propertyId, array.Get(), dict);
      break;
    }
    case UIA_LabeledByPropertyId: {
      Microsoft::WRL::ComPtr<IUIAutomationElement> node;
      if (unk && SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&node)))) {
        dict->SetString(UiaIdentifierToCondensedString(propertyId),
                        GetNodeName(node.Get()));
      }
      break;
    }
    default:
      break;
  }
}

void AccessibilityTreeFormatterUia::WriteElementArray(
    long propertyId,
    IUIAutomationElementArray* array,
    base::DictionaryValue* dict) {
  int count;
  array->get_Length(&count);
  base::string16 element_list = L"";
  for (int i = 0; i < count; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> element;
    if (SUCCEEDED(array->GetElement(i, &element))) {
      if (element_list != L"") {
        element_list += L", ";
      }
      element_list += GetNodeName(element.Get());
    }
  }
  dict->SetString(UiaIdentifierToCondensedString(propertyId), element_list);
}

base::string16 AccessibilityTreeFormatterUia::GetNodeName(
    IUIAutomationElement* uncached_node) {
  // Update the cache for this node.
  Microsoft::WRL::ComPtr<IUIAutomationElement> node;
  uncached_node->BuildUpdatedCache(element_cache_request_.Get(), &node);

  base::win::ScopedBstr name;
  base::win::ScopedVariant variant;
  if (SUCCEEDED(node->GetCachedPropertyValue(UIA_NamePropertyId,
                                             variant.Receive())) &&
      variant.type() == VT_BSTR) {
    return base::string16(variant.ptr()->bstrVal,
                          SysStringLen(variant.ptr()->bstrVal));
  }
  return L"";
}

void AccessibilityTreeFormatterUia::BuildCacheRequests() {
  // Create cache request for requesting children of a node.
  uia_->CreateCacheRequest(&children_cache_request_);
  CHECK(children_cache_request_.Get());
  children_cache_request_->put_TreeScope(TreeScope_Children);

  // Create cache request for requesting information about a node.
  uia_->CreateCacheRequest(&element_cache_request_);
  CHECK(element_cache_request_.Get());
  element_cache_request_->put_TreeScope(TreeScope_Element);

  // Caching properties allows us to use GetCachedPropertyValue.
  // The non-cached version (GetCurrentPropertyValue) may cross
  // the process boundary for each call.

  // Cache all properties.
  for (long i : properties_) {
    element_cache_request_->AddProperty(i);
  }
}

base::string16 AccessibilityTreeFormatterUia::ProcessTreeForOutput(
    const base::DictionaryValue& dict,
    base::DictionaryValue* filtered_dict_result) {
  std::unique_ptr<base::DictionaryValue> tree;
  base::string16 line;

  // Always show role, and show it first.
  base::string16 role_value;
  dict.GetString(UiaIdentifierToCondensedString(UIA_AriaRolePropertyId),
                 &role_value);
  WriteAttribute(true, base::UTF16ToUTF8(role_value), &line);
  if (filtered_dict_result) {
    filtered_dict_result->SetString(
        UiaIdentifierToStringUTF8(UIA_AriaRolePropertyId), role_value);
  }

  for (long i : properties_) {
    const std::string attribute_name = UiaIdentifierToCondensedString(i);
    const base::Value* value;
    if (!dict.Get(attribute_name, &value))
      continue;
    switch (value->type()) {
      case base::Value::Type::STRING: {
        base::string16 string_value;
        value->GetAsString(&string_value);
        bool did_pass_filters = WriteAttribute(
            false,
            base::StringPrintf(L"%ls='%ls'",
                               base::UTF8ToUTF16(attribute_name).c_str(),
                               string_value.c_str()),
            &line);
        if (filtered_dict_result && did_pass_filters)
          filtered_dict_result->SetString(attribute_name, string_value);
        break;
      }
      case base::Value::Type::BOOLEAN: {
        bool bool_value = 0;
        value->GetAsBoolean(&bool_value);
        bool did_pass_filters = WriteAttribute(
            false,
            base::StringPrintf(L"%ls=%ls",
                               base::UTF8ToUTF16(attribute_name).c_str(),
                               (bool_value ? L"true" : L"false")),
            &line);
        if (filtered_dict_result && did_pass_filters)
          filtered_dict_result->SetBoolean(attribute_name, bool_value);
        break;
      }
      case base::Value::Type::INTEGER: {
        int int_value = 0;
        value->GetAsInteger(&int_value);
        bool did_pass_filters = WriteAttribute(
            false,
            base::StringPrintf(L"%ls=%d",
                               base::UTF8ToUTF16(attribute_name).c_str(),
                               int_value),
            &line);
        if (filtered_dict_result && did_pass_filters)
          filtered_dict_result->SetInteger(attribute_name, int_value);
        break;
      }
      case base::Value::Type::DOUBLE: {
        double double_value = 0.0;
        value->GetAsDouble(&double_value);
        bool did_pass_filters = WriteAttribute(
            false,
            base::StringPrintf(L"%ls=%.2f",
                               base::UTF8ToUTF16(attribute_name).c_str(),
                               double_value),
            &line);
        if (filtered_dict_result && did_pass_filters)
          filtered_dict_result->SetDouble(attribute_name, double_value);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }
  return line;
}

const base::FilePath::StringType
AccessibilityTreeFormatterUia::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-uia-win.txt");
}

const base::FilePath::StringType
AccessibilityTreeFormatterUia::GetVersionSpecificExpectedFileSuffix() {
  if (base::win::GetVersion() == base::win::Version::VERSION_WIN7) {
    return FILE_PATH_LITERAL("-expected-uia-win7.txt");
  }
  return FILE_PATH_LITERAL("");
}

const std::string AccessibilityTreeFormatterUia::GetAllowEmptyString() {
  return "@UIA-WIN-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterUia::GetAllowString() {
  return "@UIA-WIN-ALLOW:";
}

const std::string AccessibilityTreeFormatterUia::GetDenyString() {
  return "@UIA-WIN-DENY:";
}

const std::string AccessibilityTreeFormatterUia::GetDenyNodeString() {
  return "@UIA-WIN-DENY-NODE:";
}

}  // namespace content
