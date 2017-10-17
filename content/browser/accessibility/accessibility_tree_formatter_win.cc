// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include <oleacc.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/base/win/atl_module.h"


namespace content {

class AccessibilityTreeFormatterWin : public AccessibilityTreeFormatter {
 public:
  AccessibilityTreeFormatterWin();
  ~AccessibilityTreeFormatterWin() override;

  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* start) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      base::win::ScopedComPtr<IAccessible> start,
      LONG window_x = 0,
      LONG window_y = 0);

 private:
  void RecursiveBuildAccessibilityTree(
      const base::win::ScopedComPtr<IAccessible> node,
      base::DictionaryValue* dict,
      LONG root_x,
      LONG root_y);

  const base::FilePath::StringType GetExpectedFileSuffix() override;
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
  void AddProperties(const base::win::ScopedComPtr<IAccessible>,
                     base::DictionaryValue* dict,
                     LONG root_x,
                     LONG root_y);
  void AddMSAAProperties(const base::win::ScopedComPtr<IAccessible>,
                         base::DictionaryValue* dict,
                         LONG root_x,
                         LONG root_y);
  void AddSimpleDOMNodeProperties(const base::win::ScopedComPtr<IAccessible>,
                                  base::DictionaryValue* dict);
  bool AddIA2Properties(const base::win::ScopedComPtr<IAccessible>,
                        base::DictionaryValue* dict);
  void AddIA2ActionProperties(const base::win::ScopedComPtr<IAccessible>,
                              base::DictionaryValue* dict);
  void AddIA2HypertextProperties(const base::win::ScopedComPtr<IAccessible>,
                                 base::DictionaryValue* dict);
  void AddIA2TextProperties(const base::win::ScopedComPtr<IAccessible>,
                            base::DictionaryValue* dict);
  void AddIA2TableProperties(const base::win::ScopedComPtr<IAccessible>,
                             base::DictionaryValue* dict);
  void AddIA2ValueProperties(const base::win::ScopedComPtr<IAccessible>,
                             base::DictionaryValue* dict);
  base::string16 ToString(const base::DictionaryValue& node) override;
};

// static
AccessibilityTreeFormatter* AccessibilityTreeFormatter::Create() {
  return new AccessibilityTreeFormatterWin();
}

AccessibilityTreeFormatterWin::AccessibilityTreeFormatterWin() {
  ui::win::CreateATLModuleIfNeeded();
}

AccessibilityTreeFormatterWin::~AccessibilityTreeFormatterWin() {
}

static HRESULT QuerySimpleDOMNode(IAccessible* accessible,
                                  ISimpleDOMNode** simple_dom_node) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving IAccessible2.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_ISimpleDOMNode, simple_dom_node);
}

static HRESULT QueryIAccessible2(IAccessible* accessible,
                                 IAccessible2** accessible2) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving IAccessible2.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessible2, accessible2);
}

static HRESULT QueryIAccessibleAction(IAccessible* accessible,
                                      IAccessibleAction** accessibleAction) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  if (FAILED(hr))
    return hr;

  return service_provider->QueryService(IID_IAccessibleAction,
                                        accessibleAction);
}

static HRESULT QueryIAccessibleHypertext(
    IAccessible* accessible,
    IAccessibleHypertext** accessibleHypertext) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleHypertext,
                                        accessibleHypertext);
}

static HRESULT QueryIAccessibleTable(IAccessible* accessible,
                                     IAccessibleTable** accessibleTable) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleTable, accessibleTable);
}

static HRESULT QueryIAccessibleText(IAccessible* accessible,
                                    IAccessibleText** accessibleText) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleText, accessibleText);
}

static HRESULT QueryIAccessibleValue(IAccessible* accessible,
                                     IAccessibleValue** accessibleValue) {
  // IA2 Spec dictates that IServiceProvider should be used instead of
  // QueryInterface when retrieving alternate interfaces.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  if (FAILED(hr))
    return hr;
  return service_provider->QueryService(IID_IAccessibleValue, accessibleValue);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterWin::BuildAccessibilityTree(
    BrowserAccessibility* start_node) {
  DCHECK(start_node);

  VARIANT variant_self;
  variant_self.vt = VT_I4;
  variant_self.lVal = CHILDID_SELF;
  LONG root_x, root_y, root_width, root_height;
  BrowserAccessibility* root =
      start_node->manager()->GetRootManager()->GetRoot();
  HRESULT hr = ToBrowserAccessibilityWin(root)->GetCOM()->accLocation(
      &root_x, &root_y, &root_width, &root_height, variant_self);
  DCHECK(SUCCEEDED(hr));

  base::win::ScopedComPtr<IAccessible> start_ia =
      ToBrowserAccessibilityComWin(start_node);

  return BuildAccessibilityTree(start_ia, root_x, root_y);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterWin::BuildAccessibilityTree(
    base::win::ScopedComPtr<IAccessible> start,
    LONG root_x,
    LONG root_y) {
  CHECK(start);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  RecursiveBuildAccessibilityTree(start, dict.get(), root_x, root_y);

  return dict;
}

void AccessibilityTreeFormatterWin::RecursiveBuildAccessibilityTree(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict,
    LONG root_x,
    LONG root_y) {
  AddProperties(node, dict, root_x, root_y);

  auto children = base::MakeUnique<base::ListValue>();

  LONG child_count;
  if (S_OK != node->get_accChildCount(&child_count))
    return;

  for (int index = 1; index <= child_count; ++index) {
    base::win::ScopedVariant childid_index(index);
    base::win::ScopedComPtr<IDispatch> child_dispatch;
    base::win::ScopedComPtr<IAccessible> child_accessible;
    if (S_OK ==
            node->get_accChild(childid_index, child_dispatch.GetAddressOf()) &&
        S_OK == child_dispatch.CopyTo(child_accessible.GetAddressOf())) {
      std::unique_ptr<base::DictionaryValue> child_dict(
          new base::DictionaryValue);
      RecursiveBuildAccessibilityTree(child_accessible, child_dict.get(),
                                      root_x, root_y);
      children->Append(std::move(child_dict));
    }
  }
  dict->Set(kChildrenDictAttr, std::move(children));
}

const char* const ALL_ATTRIBUTES[] = {
    "name",
    "value",
    "states",
    "attributes",
    "text_attributes",
    "ia2_hypertext",
    "currentValue",
    "minimumValue",
    "maximumValue",
    "description",
    "default_action",
    "action_name",
    "keyboard_shortcut",
    "location",
    "size",
    "index_in_parent",
    "n_relations",
    "group_level",
    "similar_items_in_group",
    "position_in_group",
    "table_rows",
    "table_columns",
    "row_index",
    "column_index",
    "n_characters",
    "caret_offset",
    "n_selections",
    "selection_start",
    "selection_end",
    "localized_extended_role",
    "inner_html",
};

void AccessibilityTreeFormatterWin::AddProperties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict,
    LONG root_x,
    LONG root_y) {
  AddMSAAProperties(node, dict, root_x, root_y);
  AddSimpleDOMNodeProperties(node, dict);
  if (AddIA2Properties(node, dict)) {
    AddIA2ActionProperties(node, dict);
    AddIA2HypertextProperties(node, dict);
    AddIA2TableProperties(node, dict);
    AddIA2TextProperties(node, dict);
    AddIA2ValueProperties(node, dict);
  }
}

void AccessibilityTreeFormatterWin::AddMSAAProperties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict,
    LONG root_x,
    LONG root_y) {
  VARIANT variant_self;
  variant_self.vt = VT_I4;
  variant_self.lVal = CHILDID_SELF;

  base::win::ScopedBstr temp_bstr;

  LONG ia_role = 0;
  VARIANT ia_role_variant;
  if (SUCCEEDED(node->get_accRole(variant_self, &ia_role_variant))) {
    if (ia_role_variant.vt == VT_I4) {
      ia_role = ia_role_variant.lVal;
      dict->SetString("role", IAccessible2RoleToString(ia_role));
    } else if (ia_role_variant.vt == VT_BSTR) {
      dict->SetString("role", base::string16(ia_role_variant.bstrVal));
    }
  }

  // If S_FALSE it means there is no name
  if (S_OK == node->get_accName(variant_self, temp_bstr.Receive())) {
    base::string16 name = base::string16(temp_bstr, temp_bstr.Length());

    // Ignore a JAWS workaround where the name of a document is " ".
    if (name != L" " || ia_role != ROLE_SYSTEM_DOCUMENT)
      dict->SetString("name", name);
  }
  temp_bstr.Reset();

  if (SUCCEEDED(node->get_accValue(variant_self, temp_bstr.Receive())))
    dict->SetString("value", base::string16(temp_bstr, temp_bstr.Length()));
  temp_bstr.Reset();

  int32_t ia_state = 0;
  VARIANT ia_state_variant;
  if (node->get_accState(variant_self, &ia_state_variant) == S_OK &&
      ia_state_variant.vt == VT_I4) {
    ia_state = ia_state_variant.intVal;

    if (true /* reduced_flakiness_mode_ */) {  // TODO
      // Avoid flakiness: these states depend on whether the window is focused
      // and the position of the mouse cursor.
      ia_state &= ~STATE_SYSTEM_HOTTRACKED;
      ia_state &= ~STATE_SYSTEM_OFFSCREEN;

      // For testing, having the focused state may also cause flakiness if the
      // window isn't in the foreground.
      ia_state &= ~STATE_SYSTEM_FOCUSED;
    }

    std::vector<base::string16> state_strings;
    IAccessibleStateToStringVector(ia_state, &state_strings);
    std::unique_ptr<base::ListValue> states(new base::ListValue());
    states->AppendStrings(state_strings);
    dict->Set("states", std::move(states));
  }

  if (SUCCEEDED(node->get_accDescription(variant_self, temp_bstr.Receive()))) {
    dict->SetString("description", base::string16(temp_bstr,
        temp_bstr.Length()));
  }
  temp_bstr.Reset();

  // |get_accDefaultAction| returns a localized string.
  if (SUCCEEDED(
          node->get_accDefaultAction(variant_self, temp_bstr.Receive()))) {
    dict->SetString("default_action", base::string16(temp_bstr,
        temp_bstr.Length()));
  }
  temp_bstr.Reset();

  if (SUCCEEDED(
          node->get_accKeyboardShortcut(variant_self, temp_bstr.Receive()))) {
    dict->SetString("keyboard_shortcut", base::string16(temp_bstr,
        temp_bstr.Length()));
  }
  temp_bstr.Reset();

  if (SUCCEEDED(node->get_accHelp(variant_self, temp_bstr.Receive())))
    dict->SetString("help", base::string16(temp_bstr, temp_bstr.Length()));

  temp_bstr.Reset();

  LONG x, y, width, height;
  if (SUCCEEDED(node->accLocation(&x, &y, &width, &height, variant_self))) {
    auto location = base::MakeUnique<base::DictionaryValue>();
    location->SetInteger("x", x - root_x);
    location->SetInteger("y", y - root_y);
    dict->Set("location", std::move(location));

    auto size = base::MakeUnique<base::DictionaryValue>();
    size->SetInteger("width", width);
    size->SetInteger("height", height);
    dict->Set("size", std::move(size));
  }
}

void AccessibilityTreeFormatterWin::AddSimpleDOMNodeProperties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict) {
  base::win::ScopedComPtr<ISimpleDOMNode> simple_dom_node;

  if (S_OK != QuerySimpleDOMNode(node.Get(), simple_dom_node.GetAddressOf()))
    return;  // No IA2Value, we are finished with this node.

  base::win::ScopedBstr temp_bstr;

  if (SUCCEEDED(simple_dom_node->get_innerHTML(temp_bstr.Receive()))) {
    dict->SetString("inner_html",
                    base::string16(temp_bstr, temp_bstr.Length()));
  }
  temp_bstr.Reset();
}

bool AccessibilityTreeFormatterWin::AddIA2Properties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict) {
  base::win::ScopedComPtr<IAccessible2> ia2;
  if (S_OK != QueryIAccessible2(node.Get(), ia2.GetAddressOf()))
    return false;  // No IA2, we are finished with this node.

  LONG ia2_role = 0;
  if (SUCCEEDED(ia2->role(&ia2_role))) {
    dict->SetKey("msaa_legacy_role", dict->Clone());
    // Overwrite MSAA role which is more limited.
    dict->SetString("role", IAccessible2RoleToString(ia2_role));
  }

  std::vector<base::string16> state_strings;
  AccessibleStates states;
  if (ia2->get_states(&states) == S_OK) {
    IAccessible2StateToStringVector(states, &state_strings);
    // Append IA2 state list to MSAA state
    base::ListValue* states_list;
    if (dict->GetList("states", &states_list))
      states_list->AppendStrings(state_strings);
  }

  base::win::ScopedBstr temp_bstr;

  if (ia2->get_attributes(temp_bstr.Receive()) == S_OK) {
    // get_attributes() returns a semicolon delimited string. Turn it into a
    // ListValue
    std::vector<base::string16> ia2_attributes = base::SplitString(
        base::string16(temp_bstr, temp_bstr.Length()), base::string16(1, ';'),
        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    std::unique_ptr<base::ListValue> attributes(new base::ListValue());
    attributes->AppendStrings(ia2_attributes);
    dict->Set("attributes", std::move(attributes));
  }
  temp_bstr.Reset();

  LONG index_in_parent;
  if (SUCCEEDED(ia2->get_indexInParent(&index_in_parent)))
    dict->SetInteger("index_in_parent", index_in_parent);

  LONG n_relations;
  if (SUCCEEDED(ia2->get_nRelations(&n_relations)))
    dict->SetInteger("n_relations", n_relations);

  LONG group_level, similar_items_in_group, position_in_group;
  // |GetGroupPosition| returns S_FALSE when no grouping information is
  // available so avoid using |SUCCEEDED|.
  if (ia2->get_groupPosition(&group_level, &similar_items_in_group,
                             &position_in_group) == S_OK) {
    dict->SetInteger("group_level", group_level);
    dict->SetInteger("similar_items_in_group", similar_items_in_group);
    dict->SetInteger("position_in_group", position_in_group);
  }

  if (SUCCEEDED(ia2->get_localizedExtendedRole(temp_bstr.Receive()))) {
    dict->SetString("localized_extended_role",
                    base::string16(temp_bstr, temp_bstr.Length()));
  }
  temp_bstr.Reset();

  return true;
}

void AccessibilityTreeFormatterWin::AddIA2ActionProperties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict) {
  base::win::ScopedComPtr<IAccessibleAction> ia2action;
  if (S_OK != QueryIAccessibleAction(node.Get(), ia2action.GetAddressOf()))
    return;  // No IA2Value, we are finished with this node.

  base::win::ScopedBstr temp_bstr;

  // |IAccessibleAction::get_name| returns a localized string.
  if (SUCCEEDED(
          ia2action->get_name(0 /* action_index */, temp_bstr.Receive()))) {
    dict->SetString("action_name",
                    base::string16(temp_bstr, temp_bstr.Length()));
  }
}

void AccessibilityTreeFormatterWin::AddIA2HypertextProperties(
    base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict) {
  base::win::ScopedComPtr<IAccessibleHypertext> ia2hyper;
  if (S_OK != QueryIAccessibleHypertext(node.Get(), ia2hyper.GetAddressOf()))
    return;  // No IA2, we are finished with this node

  base::win::ScopedBstr text_bstr;
  HRESULT hr;

  hr = ia2hyper->get_text(0, IA2_TEXT_OFFSET_LENGTH, text_bstr.Receive());
  if (FAILED(hr))
    return;

  base::string16 ia2_hypertext(text_bstr, text_bstr.Length());
  // IA2 Spec calls embedded objects hyperlinks. We stick to embeds for clarity.
  LONG number_of_embeds;
  hr = ia2hyper->get_nHyperlinks(&number_of_embeds);
  if (SUCCEEDED(hr) && number_of_embeds > 0) {
    // Replace all embedded characters with the child indices of the
    // accessibility objects they refer to.
    base::string16 embedded_character(
        1, BrowserAccessibilityComWin::kEmbeddedCharacter);
    size_t character_index = 0;
    size_t hypertext_index = 0;
    while (hypertext_index < ia2_hypertext.length()) {
      if (ia2_hypertext[hypertext_index] !=
          BrowserAccessibilityComWin::kEmbeddedCharacter) {
        ++character_index;
        ++hypertext_index;
        continue;
      }

      LONG index_of_embed;
      hr = ia2hyper->get_hyperlinkIndex(character_index, &index_of_embed);
      // S_FALSE will be returned if no embedded object is found at the given
      // embedded character offset. Exclude child index from such cases.
      LONG child_index = -1;
      if (hr == S_OK) {
        DCHECK_GE(index_of_embed, 0);
        base::win::ScopedComPtr<IAccessibleHyperlink> embedded_object;
        hr = ia2hyper->get_hyperlink(index_of_embed,
                                     embedded_object.GetAddressOf());
        DCHECK(SUCCEEDED(hr));
        base::win::ScopedComPtr<IAccessible2> ax_embed;
        hr = embedded_object.CopyTo(ax_embed.GetAddressOf());
        DCHECK(SUCCEEDED(hr));
        hr = ax_embed->get_indexInParent(&child_index);
        DCHECK(SUCCEEDED(hr));
      }

      base::string16 child_index_str(L"<obj");
      if (child_index >= 0) {
        base::StringAppendF(&child_index_str, L"%d>", child_index);
      } else {
        base::StringAppendF(&child_index_str, L">");
      }
      base::ReplaceFirstSubstringAfterOffset(
          &ia2_hypertext, hypertext_index, embedded_character, child_index_str);
      ++character_index;
      hypertext_index += child_index_str.length();
      --number_of_embeds;
    }
  }
  DCHECK_EQ(number_of_embeds, 0);

  dict->SetString("ia2_hypertext", ia2_hypertext);
}

void AccessibilityTreeFormatterWin::AddIA2TableProperties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict) {
  base::win::ScopedComPtr<IAccessibleTable> ia2table;
  if (S_OK != QueryIAccessibleTable(node.Get(), ia2table.GetAddressOf()))
    return;  // No IA2Text, we are finished with this node.

  LONG table_rows;
  if (SUCCEEDED(ia2table->get_nRows(&table_rows)))
    dict->SetInteger("table_rows", table_rows);

  LONG table_columns;
  if (SUCCEEDED(ia2table->get_nColumns(&table_columns)))
    dict->SetInteger("table_columns", table_columns);
}

void AccessibilityTreeFormatterWin::AddIA2TextProperties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict) {
  base::win::ScopedComPtr<IAccessibleText> ia2text;
  if (S_OK != QueryIAccessibleText(node.Get(), ia2text.GetAddressOf()))
    return;  // No IA2Text, we are finished with this node.

  LONG n_characters;
  if (SUCCEEDED(ia2text->get_nCharacters(&n_characters)))
    dict->SetInteger("n_characters", n_characters);

  LONG caret_offset;
  if (ia2text->get_caretOffset(&caret_offset) == S_OK)
    dict->SetInteger("caret_offset", caret_offset);

  LONG n_selections;
  if (SUCCEEDED(ia2text->get_nSelections(&n_selections))) {
    dict->SetInteger("n_selections", n_selections);
    if (n_selections > 0) {
      LONG start, end;
      if (SUCCEEDED(ia2text->get_selection(0, &start, &end))) {
        dict->SetInteger("selection_start", start);
        dict->SetInteger("selection_end", end);
      }
    }
  }

  // Handle IA2 text attributes, adding them as a list.
  // IA2 text attributes comes formatted as a single string, as follows:
  // https://wiki.linuxfoundation.org/accessibility/iaccessible2/textattributes
  std::unique_ptr<base::ListValue> text_attributes(new base::ListValue());
  LONG current_offset = 0, start_offset, end_offset;
  while (current_offset < n_characters) {
    // TODO(aleventhal) n_characters is not actually useful for ending the
    // loop, because it counts embedded object characters as more than 1,
    // meaning that it counts all the text in the subtree. However, the
    // offsets used in other IAText methods only count the embedded object
    // characters as 1.
    base::win::ScopedBstr temp_bstr;
    HRESULT hr = ia2text->get_attributes(current_offset, &start_offset,
                                         &end_offset, temp_bstr.Receive());
    // The below start_offset < current_offset check is needed because
    // nCharacters is not helpful as described above.
    // When asking for a range past the end of the string, this will occur,
    // although it's not clear whether that's desired or whether
    // S_FALSE or an error should be returned when the offset is out of range.
    if (FAILED(hr) || start_offset < current_offset)
      break;
    // DCHECK(start_offset == current_offset);  // Always at text range start.
    if (hr == S_OK && temp_bstr && wcslen(temp_bstr)) {
      // Append offset:<number>.
      base::string16 offset_str =
          base::ASCIIToUTF16("offset:") + base::IntToString16(start_offset);
      text_attributes->AppendString(offset_str);
      // Append name:value pairs.
      std::vector<base::string16> name_val_pairs =
          SplitString(base::string16(temp_bstr), base::ASCIIToUTF16(";"),
                      base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      text_attributes->AppendStrings(name_val_pairs);
    }
    current_offset = end_offset;
  }

  dict->Set("text_attributes", std::move(text_attributes));
}

void AccessibilityTreeFormatterWin::AddIA2ValueProperties(
    const base::win::ScopedComPtr<IAccessible> node,
    base::DictionaryValue* dict) {
  base::win::ScopedComPtr<IAccessibleValue> ia2value;
  if (S_OK != QueryIAccessibleValue(node.Get(), ia2value.GetAddressOf()))
    return;  // No IA2Value, we are finished with this node.

  VARIANT currentValue;
  if (ia2value->get_currentValue(&currentValue) == S_OK)
    dict->SetDouble("currentValue", V_R8(&currentValue));

  VARIANT minimumValue;
  if (ia2value->get_minimumValue(&minimumValue) == S_OK)
    dict->SetDouble("minimumValue", V_R8(&minimumValue));

  VARIANT maximumValue;
  if (ia2value->get_maximumValue(&maximumValue) == S_OK)
    dict->SetDouble("maximumValue", V_R8(&maximumValue));
}

base::string16 AccessibilityTreeFormatterWin::ToString(
    const base::DictionaryValue& dict) {
  base::string16 line;

  base::string16 role_value;
  dict.GetString("role", &role_value);
  WriteAttribute(true, base::UTF16ToUTF8(role_value), &line);

  for (const char* attribute_name : ALL_ATTRIBUTES) {
    const base::Value* value;
    if (!dict.Get(attribute_name, &value))
      continue;

    switch (value->GetType()) {
      case base::Value::Type::STRING: {
        base::string16 string_value;
        value->GetAsString(&string_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls='%ls'",
                                    base::UTF8ToUTF16(attribute_name).c_str(),
                                    string_value.c_str()),
                       &line);
        break;
      }
      case base::Value::Type::INTEGER: {
        int int_value = 0;
        value->GetAsInteger(&int_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls=%d",
                                          base::UTF8ToUTF16(
                                              attribute_name).c_str(),
                                          int_value),
                       &line);
        break;
      }
      case base::Value::Type::DOUBLE: {
        double double_value = 0.0;
        value->GetAsDouble(&double_value);
        WriteAttribute(false,
                       base::StringPrintf(L"%ls=%.2f",
                                          base::UTF8ToUTF16(
                                              attribute_name).c_str(),
                                          double_value),
                       &line);
        break;
      }
      case base::Value::Type::LIST: {
        // Currently all list values are string and are written without
        // attribute names.
        const base::ListValue* list_value;
        value->GetAsList(&list_value);
        for (base::ListValue::const_iterator it = list_value->begin();
             it != list_value->end();
             ++it) {
          base::string16 string_value;
          if (it->GetAsString(&string_value))
            WriteAttribute(false, string_value, &line);
        }
        break;
      }
      case base::Value::Type::DICTIONARY: {
        // Currently all dictionary values are coordinates.
        // Revisit this if that changes.
        const base::DictionaryValue* dict_value;
        value->GetAsDictionary(&dict_value);
        if (strcmp(attribute_name, "size") == 0) {
          WriteAttribute(false,
                         FormatCoordinates("size", "width", "height",
                                           *dict_value),
                         &line);
        } else if (strcmp(attribute_name, "location") == 0) {
          WriteAttribute(false,
                         FormatCoordinates("location", "x", "y", *dict_value),
                         &line);
        }
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
AccessibilityTreeFormatterWin::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-win.txt");
}

const std::string AccessibilityTreeFormatterWin::GetAllowEmptyString() {
  return "@WIN-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterWin::GetAllowString() {
  return "@WIN-ALLOW:";
}

const std::string AccessibilityTreeFormatterWin::GetDenyString() {
  return "@WIN-DENY:";
}

}  // namespace content
