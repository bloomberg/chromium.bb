// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_win.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/accessibility/browser_accessibility_manager_win.h"
#include "net/base/escape.h"

using webkit_glue::WebAccessibility;

// The GUID for the ISimpleDOM service is not defined in the IDL files.
// This is taken directly from the Mozilla sources
// (accessible/src/msaa/nsAccessNodeWrap.cpp) and it's also documented at:
// http://developer.mozilla.org/en/Accessibility/AT-APIs/ImplementationFeatures/MSAA

const GUID GUID_ISimpleDOM = {
    0x0c539790, 0x12e4, 0x11cf,
    0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8};

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  CComObject<BrowserAccessibilityWin>* instance;
  HRESULT hr = CComObject<BrowserAccessibilityWin>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  return instance->NewReference();
}

BrowserAccessibilityWin* BrowserAccessibility::toBrowserAccessibilityWin() {
  return static_cast<BrowserAccessibilityWin*>(this);
}

BrowserAccessibilityWin::BrowserAccessibilityWin()
    : instance_active_(false),
      ia_role_(0),
      ia_state_(0),
      ia2_role_(0),
      ia2_state_(0) {
}

BrowserAccessibilityWin::~BrowserAccessibilityWin() {
  ReleaseTree();
}

//
// IAccessible methods.
//
// Conventions:
// * Always test for instance_active_ first and return E_FAIL if it's false.
// * Always check for invalid arguments first, even if they're unused.
// * Return S_FALSE if the only output is a string argument and it's empty.
//

HRESULT BrowserAccessibilityWin::accDoDefaultAction(VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  manager_->DoDefaultAction(*target);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accHitTest(LONG x_left,
                                                 LONG y_top,
                                                 VARIANT* child) {
  if (!instance_active_)
    return E_FAIL;

  if (!child)
    return E_INVALIDARG;

  gfx::Point point(x_left, y_top);
  if (!GetBoundsRect().Contains(point)) {
    // Return S_FALSE and VT_EMPTY when the outside the object's boundaries.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  BrowserAccessibility* result = BrowserAccessibilityForPoint(point);
  if (result == this) {
    // Point is within this object.
    child->vt = VT_I4;
    child->lVal = CHILDID_SELF;
  } else {
    child->vt = VT_DISPATCH;
    child->pdispVal = result->toBrowserAccessibilityWin()->NewReference();
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accLocation(LONG* x_left, LONG* y_top,
                                                  LONG* width, LONG* height,
                                                  VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  if (!x_left || !y_top || !width || !height)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  gfx::Rect bounds = target->GetBoundsRect();
  *x_left = bounds.x();
  *y_top  = bounds.y();
  *width  = bounds.width();
  *height = bounds.height();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accNavigate(
    LONG nav_dir, VARIANT start, VARIANT* end) {
  BrowserAccessibilityWin* target = GetTargetFromChildID(start);
  if (!target)
    return E_INVALIDARG;

  if ((nav_dir == NAVDIR_LASTCHILD || nav_dir == NAVDIR_FIRSTCHILD) &&
      start.lVal != CHILDID_SELF) {
    // MSAA states that navigating to first/last child can only be from self.
    return E_INVALIDARG;
  }

  BrowserAccessibility* result = NULL;
  switch (nav_dir) {
    case NAVDIR_DOWN:
    case NAVDIR_UP:
    case NAVDIR_LEFT:
    case NAVDIR_RIGHT:
      // These directions are not implemented, matching Mozilla and IE.
      return E_NOTIMPL;
    case NAVDIR_FIRSTCHILD:
      if (!target->children_.empty())
        result = target->children_.front();
      break;
    case NAVDIR_LASTCHILD:
      if (!target->children_.empty())
        result = target->children_.back();
      break;
    case NAVDIR_NEXT:
      result = target->GetNextSibling();
      break;
    case NAVDIR_PREVIOUS:
      result = target->GetPreviousSibling();
      break;
  }

  if (!result) {
    end->vt = VT_EMPTY;
    return S_FALSE;
  }

  end->vt = VT_DISPATCH;
  end->pdispVal = result->toBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accChild(VARIANT var_child,
                                                   IDispatch** disp_child) {
  if (!instance_active_)
    return E_FAIL;

  if (!disp_child)
    return E_INVALIDARG;

  *disp_child = NULL;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_child);
  if (!target)
    return E_INVALIDARG;

  (*disp_child) = target->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accChildCount(LONG* child_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!child_count)
    return E_INVALIDARG;

  *child_count = children_.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accDefaultAction(VARIANT var_id,
                                                           BSTR* def_action) {
  if (!instance_active_)
    return E_FAIL;

  if (!def_action)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(
      WebAccessibility::ATTR_SHORTCUT, def_action);
}

STDMETHODIMP BrowserAccessibilityWin::get_accDescription(VARIANT var_id,
                                                         BSTR* desc) {
  if (!instance_active_)
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(WebAccessibility::ATTR_DESCRIPTION, desc);
}

STDMETHODIMP BrowserAccessibilityWin::get_accFocus(VARIANT* focus_child) {
  if (!instance_active_)
    return E_FAIL;

  if (!focus_child)
    return E_INVALIDARG;

  BrowserAccessibilityWin* focus = static_cast<BrowserAccessibilityWin*>(
      manager_->GetFocus(this));
  if (focus == this) {
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else if (focus == NULL) {
    focus_child->vt = VT_EMPTY;
  } else {
    focus_child->vt = VT_DISPATCH;
    focus_child->pdispVal = focus->NewReference();
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accHelp(VARIANT var_id, BSTR* help) {
  if (!instance_active_)
    return E_FAIL;

  if (!help)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(WebAccessibility::ATTR_HELP, help);
}

STDMETHODIMP BrowserAccessibilityWin::get_accKeyboardShortcut(VARIANT var_id,
                                                              BSTR* acc_key) {
  if (!instance_active_)
    return E_FAIL;

  if (!acc_key)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(WebAccessibility::ATTR_SHORTCUT, acc_key);
}

STDMETHODIMP BrowserAccessibilityWin::get_accName(VARIANT var_id, BSTR* name) {
  if (!instance_active_)
    return E_FAIL;

  if (!name)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (target->name_.empty())
    return S_FALSE;

  *name = SysAllocString(target->name_.c_str());

  DCHECK(*name);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accParent(IDispatch** disp_parent) {
  if (!instance_active_)
    return E_FAIL;

  if (!disp_parent)
    return E_INVALIDARG;

  IAccessible* parent = parent_->toBrowserAccessibilityWin();
  if (parent == NULL) {
    // This happens if we're the root of the tree;
    // return the IAccessible for the window.
    parent = manager_->toBrowserAccessibilityManagerWin()->
             GetParentWindowIAccessible();
  }

  parent->AddRef();
  *disp_parent = parent;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accRole(
    VARIANT var_id, VARIANT* role) {
  if (!instance_active_)
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (!target->role_name_.empty()) {
    role->vt = VT_BSTR;
    role->bstrVal = SysAllocString(target->role_name_.c_str());
  } else {
    role->vt = VT_I4;
    role->lVal = target->ia_role_;
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accState(VARIANT var_id,
                                                   VARIANT* state) {
  if (!instance_active_)
    return E_FAIL;

  if (!state)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  state->vt = VT_I4;
  state->lVal = target->ia_state_;
  if (manager_->GetFocus(NULL) == this)
    state->lVal |= STATE_SYSTEM_FOCUSED;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accValue(
    VARIANT var_id, BSTR* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  *value = SysAllocString(target->value_.c_str());

  DCHECK(*value);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accHelpTopic(
    BSTR* help_file, VARIANT var_id, LONG* topic_id) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_accSelection(VARIANT* selected) {
  if (!instance_active_)
    return E_FAIL;

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::accSelect(
    LONG flags_sel, VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  if (flags_sel & SELFLAG_TAKEFOCUS) {
    manager_->SetFocus(*this);
    return S_OK;
  }

  return S_FALSE;
}

//
// IAccessible2 methods.
//

STDMETHODIMP BrowserAccessibilityWin::role(LONG* role) {
  if (!instance_active_)
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  *role = ia2_role_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributes(BSTR* attributes) {
  if (!instance_active_)
    return E_FAIL;

  if (!attributes)
    return E_INVALIDARG;

  // Follow Firefox's convention, which is to return a set of key-value pairs
  // separated by semicolons, with a colon between the key and the value.
  string16 str;
  for (unsigned int i = 0; i < html_attributes_.size(); i++) {
    if (i != 0)
      str += L';';
    str += Escape(html_attributes_[i].first);
    str += L':';
    str += Escape(html_attributes_[i].second);
  }

  if (str.empty())
    return S_FALSE;

  *attributes = SysAllocString(str.c_str());
  DCHECK(*attributes);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_states(AccessibleStates* states) {
  if (!instance_active_)
    return E_FAIL;

  if (!states)
    return E_INVALIDARG;

  *states = ia2_state_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_uniqueID(LONG* unique_id) {
  if (!instance_active_)
    return E_FAIL;

  if (!unique_id)
    return E_INVALIDARG;

  *unique_id = child_id_;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_windowHandle(HWND* window_handle) {
  if (!instance_active_)
    return E_FAIL;

  if (!window_handle)
    return E_INVALIDARG;

  *window_handle = manager_->GetParentView();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_indexInParent(LONG* index_in_parent) {
  if (!instance_active_)
    return E_FAIL;

  if (!index_in_parent)
    return E_INVALIDARG;

  *index_in_parent = index_in_parent_;
  return S_OK;
}

//
// IAccessibleImage methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_description(BSTR* desc) {
  if (!instance_active_)
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DESCRIPTION, desc);
}

STDMETHODIMP BrowserAccessibilityWin::get_imagePosition(
    enum IA2CoordinateType coordinate_type, LONG* x, LONG* y) {
  if (!instance_active_)
    return E_FAIL;

  if (!x || !y)
    return E_INVALIDARG;

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    HWND parent_hwnd = manager_->GetParentView();
    POINT top_left = {0, 0};
    ::ClientToScreen(parent_hwnd, &top_left);
    *x = location_.x + top_left.x;
    *y = location_.y + top_left.y;
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    *x = location_.x;
    *y = location_.y;
    if (parent_) {
      *x -= parent_->location().x;
      *y -= parent_->location().y;
    }
  } else {
    return E_INVALIDARG;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_imageSize(LONG* height, LONG* width) {
  if (!instance_active_)
    return E_FAIL;

  if (!height || !width)
    return E_INVALIDARG;

  *height = location_.height;
  *width = location_.width;
  return S_OK;
}

//
// IAccessibleText methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nCharacters(LONG* n_characters) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_characters)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD) {
    *n_characters = value_.length();
  } else {
    *n_characters = name_.length();
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_caretOffset(LONG* offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD) {
    int sel_start = 0;
    if (GetAttributeAsInt(WebAccessibility::ATTR_TEXT_SEL_START, &sel_start)) {
      *offset = sel_start;
    } else {
      *offset = 0;
    }
  } else {
    *offset = 0;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelections(LONG* n_selections) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_selections)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetAttributeAsInt(WebAccessibility::ATTR_TEXT_SEL_START, &sel_start) &&
        GetAttributeAsInt(WebAccessibility::ATTR_TEXT_SEL_END, &sel_end) &&
        sel_start != sel_end) {
      *n_selections = 1;
    } else {
      *n_selections = 0;
    }
  } else {
    *n_selections = 0;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selection(LONG selection_index,
                                                    LONG* start_offset,
                                                    LONG* end_offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || selection_index != 0)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetAttributeAsInt(WebAccessibility::ATTR_TEXT_SEL_START, &sel_start) &&
        GetAttributeAsInt(WebAccessibility::ATTR_TEXT_SEL_END, &sel_end)) {
      *start_offset = sel_start;
      *end_offset = sel_end;
    } else {
      *start_offset = 0;
      *end_offset = 0;
    }
  } else {
    *start_offset = 0;
    *end_offset = 0;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_text(
    LONG start_offset, LONG end_offset, BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  const string16& text_str = TextForIAccessibleText();

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset) {
    LONG tmp = start_offset;
    start_offset = end_offset;
    end_offset = tmp;
  }

  // The spec does not allow the start or end offsets to be out or range;
  // we must return an error if so.
  LONG len = text_str.length();
  if (start_offset < 0)
    return E_INVALIDARG;
  if (end_offset > len)
    return E_INVALIDARG;

  string16 substr = text_str.substr(start_offset, end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(substr.c_str());
  DCHECK(*text);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_textAtOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(text_str, boundary_type, offset, -1);
  *end_offset = FindBoundary(text_str, boundary_type, offset, 1);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textBeforeOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(text_str, boundary_type, offset, -1);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textAfterOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const string16& text_str = TextForIAccessibleText();

  *start_offset = offset;
  *end_offset = FindBoundary(text_str, boundary_type, offset, 1);
  return get_text(*start_offset, *end_offset, text);
}

//
// ISimpleDOMDocument methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_URL(BSTR* url) {
  if (!instance_active_)
    return E_FAIL;

  if (!url)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_URL, url);
}

STDMETHODIMP BrowserAccessibilityWin::get_title(BSTR* title) {
  if (!instance_active_)
    return E_FAIL;

  if (!title)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_TITLE, title);
}

STDMETHODIMP BrowserAccessibilityWin::get_mimeType(BSTR* mime_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!mime_type)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_MIMETYPE, mime_type);
}

STDMETHODIMP BrowserAccessibilityWin::get_docType(BSTR* doc_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!doc_type)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_DOCTYPE, doc_type);
}

//
// ISimpleDOMNode methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nodeInfo(
    BSTR* node_name,
    short* name_space_id,
    BSTR* node_value,
    unsigned int* num_children,
    unsigned int* unique_id,
    unsigned short* node_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!node_name || !name_space_id || !node_value || !num_children ||
      !unique_id || !node_type) {
    return E_INVALIDARG;
  }

  string16 tag;
  if (GetAttribute(WebAccessibility::ATTR_HTML_TAG, &tag))
    *node_name = SysAllocString(tag.c_str());
  else
    *node_name = NULL;

  *name_space_id = 0;
  *node_value = SysAllocString(value_.c_str());
  *num_children = children_.size();
  *unique_id = child_id_;

  if (ia_role_ == ROLE_SYSTEM_DOCUMENT) {
    *node_type = NODETYPE_DOCUMENT;
  } else if (ia_role_ == ROLE_SYSTEM_TEXT &&
             ((ia2_state_ & IA2_STATE_EDITABLE) == 0)) {
    *node_type = NODETYPE_TEXT;
  } else {
    *node_type = NODETYPE_ELEMENT;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributes(
    unsigned short max_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values,
    unsigned short* num_attribs) {
  if (!instance_active_)
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values || !num_attribs)
    return E_INVALIDARG;

  *num_attribs = max_attribs;
  if (*num_attribs > html_attributes_.size())
    *num_attribs = html_attributes_.size();

  for (unsigned short i = 0; i < *num_attribs; ++i) {
    attrib_names[i] = SysAllocString(html_attributes_[i].first.c_str());
    name_space_id[i] = 0;
    attrib_values[i] = SysAllocString(html_attributes_[i].second.c_str());
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributesForNames(
    unsigned short num_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values) {
  if (!instance_active_)
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values)
    return E_INVALIDARG;

  for (unsigned short i = 0; i < num_attribs; ++i) {
    name_space_id[i] = 0;
    bool found = false;
    string16 name = (LPCWSTR)attrib_names[i];
    for (unsigned int j = 0;  j < html_attributes_.size(); ++j) {
      if (html_attributes_[j].first == name) {
        attrib_values[i] = SysAllocString(html_attributes_[j].second.c_str());
        found = true;
        break;
      }
    }
    if (!found) {
      attrib_values[i] = NULL;
    }
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_computedStyle(
    unsigned short max_style_properties,
    boolean use_alternate_view,
    BSTR *style_properties,
    BSTR *style_values,
    unsigned short *num_style_properties)  {
  if (!instance_active_)
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  if (max_style_properties == 0 ||
      !HasAttribute(WebAccessibility::ATTR_DISPLAY)) {
    *num_style_properties = 0;
    return S_OK;
  }

  string16 display;
  GetAttribute(WebAccessibility::ATTR_DISPLAY, &display);
  *num_style_properties = 1;
  style_properties[0] = SysAllocString(L"display");
  style_values[0] = SysAllocString(display.c_str());

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_computedStyleForProperties(
    unsigned short num_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values) {
  if (!instance_active_)
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  for (unsigned short i = 0; i < num_style_properties; i++) {
    string16 name = (LPCWSTR)style_properties[i];
    StringToLowerASCII(&name);
    if (name == L"display") {
      string16 display;
      GetAttribute(WebAccessibility::ATTR_DISPLAY, &display);
      style_values[i] = SysAllocString(display.c_str());
    } else {
      style_values[i] = NULL;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollTo(boolean placeTopLeft) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_parentNode(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  *node = parent_->toBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_firstChild(ISimpleDOMNode** node)  {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.size()) {
    *node = children_[0]->toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_lastChild(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.size()) {
    *node = children_[children_.size() - 1]->toBrowserAccessibilityWin()->
        NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_previousSibling(
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (parent_ && index_in_parent_ > 0) {
    *node = parent_->children()[index_in_parent_ - 1]->
        toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_nextSibling(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (parent_ &&
      index_in_parent_ >= 0 &&
      index_in_parent_ < static_cast<int>(parent_->children().size()) - 1) {
    *node = parent_->children()[index_in_parent_ + 1]->
        toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_childAt(
    unsigned int child_index,
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (child_index < children_.size()) {
    *node = children_[child_index]->toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

//
// ISimpleDOMText methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_domText(BSTR* dom_text) {
  if (!instance_active_)
    return E_FAIL;

  if (!dom_text)
    return E_INVALIDARG;

  if (name_.empty())
    return S_FALSE;

  *dom_text = SysAllocString(name_.c_str());
  DCHECK(*dom_text);
  return S_OK;
}

//
// IServiceProvider methods.
//

STDMETHODIMP BrowserAccessibilityWin::QueryService(
    REFGUID guidService, REFIID riid, void** object) {
  if (!instance_active_)
    return E_FAIL;

  if (guidService == IID_IAccessible ||
      guidService == IID_IAccessible2 ||
      guidService == IID_IAccessibleImage ||
      guidService == IID_IAccessibleText ||
      guidService == IID_ISimpleDOMDocument ||
      guidService == IID_ISimpleDOMNode ||
      guidService == IID_ISimpleDOMText ||
      guidService == GUID_ISimpleDOM) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

//
// CComObjectRootEx methods.
//

HRESULT WINAPI BrowserAccessibilityWin::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID iid,
    void** object) {
  if (iid == IID_IAccessibleText) {
    if (ia_role_ != ROLE_SYSTEM_LINK && ia_role_ != ROLE_SYSTEM_TEXT) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleImage) {
    if (ia_role_ != ROLE_SYSTEM_GRAPHIC) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_ISimpleDOMDocument) {
    if (ia_role_ != ROLE_SYSTEM_DOCUMENT) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  }

  return CComObjectRootBase::InternalQueryInterface(
      this_ptr, entries, iid, object);
}

//
// Private methods.
//

// Initialize this object and mark it as active.
void BrowserAccessibilityWin::Initialize() {
  InitRoleAndState();

  // Expose headings levels to NVDA with the "level" object attribute.
  if (role_ == WebAccessibility::ROLE_HEADING && role_name_.size() == 2 &&
          IsAsciiDigit(role_name_[1])) {
    html_attributes_.push_back(std::make_pair(L"level", role_name_.substr(1)));
  }

  // Expose the "display" object attribute.
  string16 display;
  if (GetAttribute(WebAccessibility::ATTR_DISPLAY, &display))
    html_attributes_.push_back(std::make_pair(L"display", display));

  // If this is static text, put the text in the name rather than the value.
  if (role_ == WebAccessibility::ROLE_STATIC_TEXT && name_.empty())
    name_.swap(value_);

  // If this object doesn't have a name but it does have a description,
  // use the description as its name - because some screen readers only
  // announce the name.
  if (name_.empty() && HasAttribute(WebAccessibility::ATTR_DESCRIPTION))
    GetAttribute(WebAccessibility::ATTR_DESCRIPTION, &name_);

  instance_active_ = true;
}

// Mark this object as inactive, and remove references to all children.
// When no other clients hold any references to this object it will be
// deleted, and in the meantime, calls to any methods will return E_FAIL.
void BrowserAccessibilityWin::ReleaseTree() {
  if (!instance_active_)
    return;

  // Mark this object as inactive, so calls to all COM methods will return
  // failure.
  instance_active_ = false;

  BrowserAccessibility::ReleaseTree();
}

void BrowserAccessibilityWin::ReleaseReference() {
  Release();
}


BrowserAccessibilityWin* BrowserAccessibilityWin::NewReference() {
  AddRef();
  return this;
}

BrowserAccessibilityWin* BrowserAccessibilityWin::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (var_id.vt != VT_I4)
    return NULL;

  LONG child_id = var_id.lVal;
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 && child_id <= static_cast<LONG>(children_.size()))
    return children_[child_id - 1]->toBrowserAccessibilityWin();

  return manager_->GetFromChildID(child_id)->toBrowserAccessibilityWin();
}

HRESULT BrowserAccessibilityWin::GetAttributeAsBstr(
    WebAccessibility::Attribute attribute, BSTR* value_bstr) {
  string16 str;

  if (!GetAttribute(attribute, &str))
    return S_FALSE;

  if (str.empty())
    return S_FALSE;

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);

  return S_OK;
}

string16 BrowserAccessibilityWin::Escape(string16 str) {
  return EscapeQueryParamValueUTF8(str, false);
}

const string16& BrowserAccessibilityWin::TextForIAccessibleText() {
  if (role_ == WebAccessibility::ROLE_TEXT_FIELD) {
    return value_;
  } else {
    return name_;
  }
}

LONG BrowserAccessibilityWin::FindBoundary(
    const string16& text,
    IA2TextBoundaryType boundary,
    LONG start_offset,
    LONG direction) {
  LONG text_size = static_cast<LONG>(text.size());
  DCHECK((start_offset >= 0 && start_offset <= text_size) ||
         start_offset == IA2_TEXT_OFFSET_LENGTH ||
         start_offset == IA2_TEXT_OFFSET_CARET);
  DCHECK(direction == 1 || direction == -1);

  if (start_offset == IA2_TEXT_OFFSET_LENGTH) {
    start_offset = text_size;
  } else if (start_offset == IA2_TEXT_OFFSET_CARET) {
    get_caretOffset(&start_offset);
  }

  if (boundary == IA2_TEXT_BOUNDARY_CHAR) {
    if (direction == 1 && start_offset < text_size)
      return start_offset + 1;
    else
      return start_offset;
  }

  LONG result = start_offset;
  for (;;) {
    LONG pos;
    if (direction == 1) {
      if (result >= text_size)
        return text_size;
      pos = result;
    } else {
      if (result <= 0)
        return 0;
      pos = result - 1;
    }

    switch (boundary) {
      case IA2_TEXT_BOUNDARY_WORD:
        if (IsWhitespace(text[pos]))
          return result;
        break;
      case IA2_TEXT_BOUNDARY_LINE:
      case IA2_TEXT_BOUNDARY_PARAGRAPH:
        if (text[pos] == '\n')
          return result;
      case IA2_TEXT_BOUNDARY_SENTENCE:
        // Note that we don't actually have to implement sentence support;
        // currently IAccessibleText functions return S_FALSE so that
        // screenreaders will handle it on their own.
        if ((text[pos] == '.' || text[pos] == '!' || text[pos] == '?') &&
            (pos == text_size - 1 || IsWhitespace(text[pos + 1]))) {
          return result;
        }
      case IA2_TEXT_BOUNDARY_ALL:
      default:
        break;
    }

    if (direction > 0) {
      result++;
    } else if (direction < 0) {
      result--;
    } else {
      NOTREACHED();
      return result;
    }
  }
}

void BrowserAccessibilityWin::InitRoleAndState() {
  ia_state_ = 0;
  ia2_state_ = IA2_STATE_OPAQUE;

  if ((state_ >> WebAccessibility::STATE_CHECKED) & 1)
    ia_state_ |= STATE_SYSTEM_CHECKED;
  if ((state_ >> WebAccessibility::STATE_COLLAPSED) & 1)
    ia_state_|= STATE_SYSTEM_COLLAPSED;
  if ((state_ >> WebAccessibility::STATE_EXPANDED) & 1)
    ia_state_|= STATE_SYSTEM_EXPANDED;
  if ((state_ >> WebAccessibility::STATE_FOCUSABLE) & 1)
    ia_state_|= STATE_SYSTEM_FOCUSABLE;
  if ((state_ >> WebAccessibility::STATE_HASPOPUP) & 1)
    ia_state_|= STATE_SYSTEM_HASPOPUP;
  if ((state_ >> WebAccessibility::STATE_HOTTRACKED) & 1)
    ia_state_|= STATE_SYSTEM_HOTTRACKED;
  if ((state_ >> WebAccessibility::STATE_INDETERMINATE) & 1)
    ia_state_|= STATE_SYSTEM_INDETERMINATE;
  if ((state_ >> WebAccessibility::STATE_INVISIBLE) & 1)
    ia_state_|= STATE_SYSTEM_INVISIBLE;
  if ((state_ >> WebAccessibility::STATE_LINKED) & 1)
    ia_state_|= STATE_SYSTEM_LINKED;
  if ((state_ >> WebAccessibility::STATE_MULTISELECTABLE) & 1)
    ia_state_|= STATE_SYSTEM_MULTISELECTABLE;
  // TODO(ctguil): Support STATE_SYSTEM_EXTSELECTABLE/accSelect.
  if ((state_ >> WebAccessibility::STATE_OFFSCREEN) & 1)
    ia_state_|= STATE_SYSTEM_OFFSCREEN;
  if ((state_ >> WebAccessibility::STATE_PRESSED) & 1)
    ia_state_|= STATE_SYSTEM_PRESSED;
  if ((state_ >> WebAccessibility::STATE_PROTECTED) & 1)
    ia_state_|= STATE_SYSTEM_PROTECTED;
  if ((state_ >> WebAccessibility::STATE_SELECTABLE) & 1)
    ia_state_|= STATE_SYSTEM_SELECTABLE;
  if ((state_ >> WebAccessibility::STATE_SELECTED) & 1)
    ia_state_|= STATE_SYSTEM_SELECTED;
  if ((state_ >> WebAccessibility::STATE_READONLY) & 1)
    ia_state_|= STATE_SYSTEM_READONLY;
  if ((state_ >> WebAccessibility::STATE_TRAVERSED) & 1)
    ia_state_|= STATE_SYSTEM_TRAVERSED;
  if ((state_ >> WebAccessibility::STATE_BUSY) & 1)
    ia_state_|= STATE_SYSTEM_BUSY;
  if ((state_ >> WebAccessibility::STATE_UNAVAILABLE) & 1)
    ia_state_|= STATE_SYSTEM_UNAVAILABLE;

  string16 html_tag;
  GetAttribute(WebAccessibility::ATTR_HTML_TAG, &html_tag);
  ia_role_ = 0;
  ia2_role_ = 0;
  switch (role_) {
    case WebAccessibility::ROLE_ALERT:
    case WebAccessibility::ROLE_ALERT_DIALOG:
      ia_role_ = ROLE_SYSTEM_ALERT;
      break;
    case WebAccessibility::ROLE_APPLICATION:
      ia_role_ = ROLE_SYSTEM_APPLICATION;
      break;
    case WebAccessibility::ROLE_ARTICLE:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_BUSY_INDICATOR:
      ia_role_ = ROLE_SYSTEM_ANIMATION;
      break;
    case WebAccessibility::ROLE_BUTTON:
      ia_role_ = ROLE_SYSTEM_PUSHBUTTON;
      break;
    case WebAccessibility::ROLE_CELL:
      ia_role_ = ROLE_SYSTEM_CELL;
      break;
    case WebAccessibility::ROLE_CHECKBOX:
      ia_role_ = ROLE_SYSTEM_CHECKBUTTON;
      break;
    case WebAccessibility::ROLE_COLOR_WELL:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_COLOR_CHOOSER;
      break;
    case WebAccessibility::ROLE_COLUMN:
      ia_role_ = ROLE_SYSTEM_COLUMN;
      break;
    case WebAccessibility::ROLE_COLUMN_HEADER:
      ia_role_ = ROLE_SYSTEM_COLUMNHEADER;
      break;
    case WebAccessibility::ROLE_COMBO_BOX:
      ia_role_ = ROLE_SYSTEM_COMBOBOX;
      break;
    case WebAccessibility::ROLE_DEFINITION_LIST_DEFINITION:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_PARAGRAPH;
      break;
    case WebAccessibility::ROLE_DEFINITION_LIST_TERM:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      break;
    case WebAccessibility::ROLE_DIALOG:
      ia_role_ = ROLE_SYSTEM_DIALOG;
      break;
    case WebAccessibility::ROLE_DISCLOSURE_TRIANGLE:
      ia_role_ = ROLE_SYSTEM_OUTLINEBUTTON;
      break;
    case WebAccessibility::ROLE_DOCUMENT:
    case WebAccessibility::ROLE_WEB_AREA:
      ia_role_ = ROLE_SYSTEM_DOCUMENT;
      ia_state_|= STATE_SYSTEM_READONLY;
      ia_state_|= STATE_SYSTEM_FOCUSABLE;
      break;
    case WebAccessibility::ROLE_EDITABLE_TEXT:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case WebAccessibility::ROLE_GRID:
      ia_role_ = ROLE_SYSTEM_TABLE;
      break;
    case WebAccessibility::ROLE_GROUP:
      if (html_tag == L"li") {
        ia_role_ = ROLE_SYSTEM_LISTITEM;
      } else {
        if (html_tag.empty())
          role_name_ = L"div";
        else
          role_name_ = html_tag;
        ia2_role_ = IA2_ROLE_SECTION;
      }
      break;
    case WebAccessibility::ROLE_GROW_AREA:
      ia_role_ = ROLE_SYSTEM_GRIP;
      break;
    case WebAccessibility::ROLE_HEADING:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_HEADING;
      break;
    case WebAccessibility::ROLE_IMAGE:
      ia_role_ = ROLE_SYSTEM_GRAPHIC;
      break;
    case WebAccessibility::ROLE_IMAGE_MAP:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_IMAGE_MAP;
      break;
    case WebAccessibility::ROLE_IMAGE_MAP_LINK:
      ia_role_ = ROLE_SYSTEM_LINK;
      ia_state_|= STATE_SYSTEM_LINKED;
      break;
    case WebAccessibility::ROLE_LANDMARK_APPLICATION:
    case WebAccessibility::ROLE_LANDMARK_BANNER:
    case WebAccessibility::ROLE_LANDMARK_COMPLEMENTARY:
    case WebAccessibility::ROLE_LANDMARK_CONTENTINFO:
    case WebAccessibility::ROLE_LANDMARK_MAIN:
    case WebAccessibility::ROLE_LANDMARK_NAVIGATION:
    case WebAccessibility::ROLE_LANDMARK_SEARCH:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_LINK:
    case WebAccessibility::ROLE_WEBCORE_LINK:
      ia_role_ = ROLE_SYSTEM_LINK;
      ia_state_|= STATE_SYSTEM_LINKED;
      break;
    case WebAccessibility::ROLE_LIST:
      ia_role_ = ROLE_SYSTEM_LIST;
      break;
    case WebAccessibility::ROLE_LISTBOX:
      ia_role_ = ROLE_SYSTEM_LIST;
      break;
    case WebAccessibility::ROLE_LISTBOX_OPTION:
    case WebAccessibility::ROLE_LIST_ITEM:
    case WebAccessibility::ROLE_LIST_MARKER:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      break;
    case WebAccessibility::ROLE_MATH:
      ia_role_ = ROLE_SYSTEM_EQUATION;
      break;
    case WebAccessibility::ROLE_MENU:
    case WebAccessibility::ROLE_MENU_BUTTON:
      ia_role_ = ROLE_SYSTEM_MENUPOPUP;
      break;
    case WebAccessibility::ROLE_MENU_BAR:
      ia_role_ = ROLE_SYSTEM_MENUBAR;
      break;
    case WebAccessibility::ROLE_MENU_ITEM:
    case WebAccessibility::ROLE_MENU_LIST_OPTION:
      ia_role_ = ROLE_SYSTEM_MENUITEM;
      break;
    case WebAccessibility::ROLE_MENU_LIST_POPUP:
      ia_role_ = ROLE_SYSTEM_MENUPOPUP;
      break;
    case WebAccessibility::ROLE_NOTE:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_NOTE;
      break;
    case WebAccessibility::ROLE_OUTLINE:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      break;
    case WebAccessibility::ROLE_POPUP_BUTTON:
      ia_role_ = ROLE_SYSTEM_COMBOBOX;
      break;
    case WebAccessibility::ROLE_PROGRESS_INDICATOR:
      ia_role_ = ROLE_SYSTEM_PROGRESSBAR;
      break;
    case WebAccessibility::ROLE_RADIO_BUTTON:
      ia_role_ = ROLE_SYSTEM_RADIOBUTTON;
      break;
    case WebAccessibility::ROLE_RADIO_GROUP:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_REGION:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_ROW:
      ia_role_ = ROLE_SYSTEM_ROW;
      break;
    case WebAccessibility::ROLE_ROW_HEADER:
      ia_role_ = ROLE_SYSTEM_ROWHEADER;
      break;
    case WebAccessibility::ROLE_RULER:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_RULER;
      break;
    case WebAccessibility::ROLE_SCROLLAREA:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SCROLL_PANE;
      break;
    case WebAccessibility::ROLE_SCROLLBAR:
      ia_role_ = ROLE_SYSTEM_SCROLLBAR;
      break;
    case WebAccessibility::ROLE_SLIDER:
      ia_role_ = ROLE_SYSTEM_SLIDER;
      break;
    case WebAccessibility::ROLE_SPLIT_GROUP:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SPLIT_PANE;
      break;
    case WebAccessibility::ROLE_ANNOTATION:
    case WebAccessibility::ROLE_STATIC_TEXT:
      ia_role_ = ROLE_SYSTEM_TEXT;
      break;
    case WebAccessibility::ROLE_STATUS:
      ia_role_ = ROLE_SYSTEM_STATUSBAR;
      break;
    case WebAccessibility::ROLE_SPLITTER:
      ia_role_ = ROLE_SYSTEM_SEPARATOR;
      break;
    case WebAccessibility::ROLE_TAB:
      ia_role_ = ROLE_SYSTEM_PAGETAB;
      break;
    case WebAccessibility::ROLE_TABLE:
      ia_role_ = ROLE_SYSTEM_TABLE;
      break;
    case WebAccessibility::ROLE_TABLE_HEADER_CONTAINER:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_TAB_GROUP:
    case WebAccessibility::ROLE_TAB_LIST:
    case WebAccessibility::ROLE_TAB_PANEL:
      ia_role_ = ROLE_SYSTEM_PAGETABLIST;
      break;
    case WebAccessibility::ROLE_TEXTAREA:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_MULTI_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case WebAccessibility::ROLE_TEXT_FIELD:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case WebAccessibility::ROLE_TIMER:
      ia_role_ = ROLE_SYSTEM_CLOCK;
      break;
    case WebAccessibility::ROLE_TOOLBAR:
      ia_role_ = ROLE_SYSTEM_TOOLBAR;
      break;
    case WebAccessibility::ROLE_TOOLTIP:
      ia_role_ = ROLE_SYSTEM_TOOLTIP;
      break;
    case WebAccessibility::ROLE_TREE:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      break;
    case WebAccessibility::ROLE_TREE_GRID:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      break;
    case WebAccessibility::ROLE_TREE_ITEM:
      ia_role_ = ROLE_SYSTEM_OUTLINEITEM;
      break;
    case WebAccessibility::ROLE_WINDOW:
      ia_role_ = ROLE_SYSTEM_WINDOW;
      break;

    // TODO(dmazzoni): figure out the proper MSAA role for all of these.
    case WebAccessibility::ROLE_BROWSER:
    case WebAccessibility::ROLE_DIRECTORY:
    case WebAccessibility::ROLE_DRAWER:
    case WebAccessibility::ROLE_HELP_TAG:
    case WebAccessibility::ROLE_IGNORED:
    case WebAccessibility::ROLE_INCREMENTOR:
    case WebAccessibility::ROLE_LOG:
    case WebAccessibility::ROLE_MARQUEE:
    case WebAccessibility::ROLE_MATTE:
    case WebAccessibility::ROLE_RULER_MARKER:
    case WebAccessibility::ROLE_SHEET:
    case WebAccessibility::ROLE_SLIDER_THUMB:
    case WebAccessibility::ROLE_SYSTEM_WIDE:
    case WebAccessibility::ROLE_VALUE_INDICATOR:
    default:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      break;
  }

  // The role should always be set.
  DCHECK(!role_name_.empty() || ia_role_);

  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!ia2_role_)
    ia2_role_ = ia_role_;
}
