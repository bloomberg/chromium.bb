// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility_win.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_accessibility_manager_win.h"
#include "net/base/escape.h"

using webkit_glue::WebAccessibility;

BrowserAccessibility::BrowserAccessibility()
    : manager_(NULL),
      parent_(NULL),
      child_id_(-1),
      index_in_parent_(-1),
      renderer_id_(-1),
      instance_active_(false) {
}

BrowserAccessibility::~BrowserAccessibility() {
  InactivateTree();
}

void BrowserAccessibility::Initialize(
    BrowserAccessibilityManager* manager,
    BrowserAccessibility* parent,
    LONG child_id,
    LONG index_in_parent,
    const webkit_glue::WebAccessibility& src) {
  manager_ = manager;
  parent_ = parent;
  child_id_ = child_id;
  index_in_parent_ = index_in_parent;

  renderer_id_ = src.id;
  name_ = src.name;
  value_ = src.value;
  attributes_ = src.attributes;
  html_attributes_ = src.html_attributes;
  location_ = src.location;
  src_role_ = src.role;
  InitRoleAndState(src.role, src.state);

  // Expose headings levels to NVDA with the "level" object attribute.
  if (src.role == WebAccessibility::ROLE_HEADING && role_name_.size() == 2 &&
          IsAsciiDigit(role_name_[1])) {
    html_attributes_.push_back(std::make_pair(L"level", role_name_.substr(1)));
  }

  // If this object doesn't have a name but it does have a description,
  // use the description as its name - because some screen readers only
  // announce the name.
  if (name_.empty() && HasAttribute(WebAccessibility::ATTR_DESCRIPTION)) {
    GetAttribute(WebAccessibility::ATTR_DESCRIPTION, &name_);
  }

  instance_active_ = true;
}

void BrowserAccessibility::AddChild(BrowserAccessibility* child) {
  children_.push_back(child);
}

void BrowserAccessibility::InactivateTree() {
  if (!instance_active_)
    return;

  // Mark this object as inactive, so calls to all COM methods will return
  // failure.
  instance_active_ = false;

  // Now we can safely call InactivateTree on our children and remove
  // references to them, so that as much of the tree as possible will be
  // destroyed now - however, nodes that still have references to them
  // might stick around a while until all clients have released them.
  for (std::vector<BrowserAccessibility*>::iterator iter =
           children_.begin();
       iter != children_.end(); ++iter) {
    (*iter)->InactivateTree();
    (*iter)->Release();
  }
  children_.clear();
  manager_->Remove(child_id_);
}

bool BrowserAccessibility::IsDescendantOf(BrowserAccessibility* ancestor) {
  if (this == ancestor) {
    return true;
  } else if (parent_) {
    return parent_->IsDescendantOf(ancestor);
  }

  return false;
}

BrowserAccessibility* BrowserAccessibility::GetParent() {
  return parent_;
}

uint32 BrowserAccessibility::GetChildCount() {
  return children_.size();
}

BrowserAccessibility* BrowserAccessibility::GetChild(uint32 child_index) {
  DCHECK(child_index >= 0 && child_index < children_.size());
  return children_[child_index];
}

BrowserAccessibility* BrowserAccessibility::GetPreviousSibling() {
  if (parent_ && index_in_parent_ > 0)
    return parent_->children_[index_in_parent_ - 1];

  return NULL;
}

BrowserAccessibility* BrowserAccessibility::GetNextSibling() {
  if (parent_ &&
      index_in_parent_ >= 0 &&
      index_in_parent_ < static_cast<int>(parent_->children_.size() - 1)) {
    return parent_->children_[index_in_parent_ + 1];
  }

  return NULL;
}

void BrowserAccessibility::ReplaceChild(
    const BrowserAccessibility* old_acc, BrowserAccessibility* new_acc) {
  DCHECK_EQ(children_[old_acc->index_in_parent_], old_acc);

  old_acc = children_[old_acc->index_in_parent_];
  children_[old_acc->index_in_parent_] = new_acc;
}

BrowserAccessibility* BrowserAccessibility::NewReference() {
  AddRef();
  return this;
}

//
// IAccessible methods.
//
// Conventions:
// * Always test for instance_active_ first and return E_FAIL if it's false.
// * Always check for invalid arguments first, even if they're unused.
// * Return S_FALSE if the only output is a string argument and it's empty.
//

HRESULT BrowserAccessibility::accDoDefaultAction(VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  manager_->DoDefaultAction(*target);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::accHitTest(LONG x_left, LONG y_top,
                                              VARIANT* child) {
  if (!instance_active_)
    return E_FAIL;

  if (!child)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::accLocation(LONG* x_left, LONG* y_top,
                                               LONG* width, LONG* height,
                                               VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  if (!x_left || !y_top || !width || !height)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  // Find the top left corner of the containing window in screen coords, and
  // adjust the output position by this amount.
  HWND parent_hwnd = manager_->GetParentHWND();
  POINT top_left = {0, 0};
  ::ClientToScreen(parent_hwnd, &top_left);

  *x_left = target->location_.x + top_left.x;
  *y_top  = target->location_.y + top_left.y;
  *width  = target->location_.width;
  *height = target->location_.height;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::accNavigate(
    LONG nav_dir, VARIANT start, VARIANT* end) {
  BrowserAccessibility* target = GetTargetFromChildID(start);
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
      if (target->children_.size() > 0)
        result = target->children_[0];
      break;
    case NAVDIR_LASTCHILD:
      if (target->children_.size() > 0)
        result = target->children_[target->children_.size() - 1];
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
  end->pdispVal = result->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accChild(VARIANT var_child,
                                                IDispatch** disp_child) {
  if (!instance_active_)
    return E_FAIL;

  if (!disp_child)
    return E_INVALIDARG;

  *disp_child = NULL;

  BrowserAccessibility* target = GetTargetFromChildID(var_child);
  if (!target)
    return E_INVALIDARG;

  (*disp_child) = target->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accChildCount(LONG* child_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!child_count)
    return E_INVALIDARG;

  *child_count = children_.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accDefaultAction(VARIANT var_id,
                                                        BSTR* def_action) {
  if (!instance_active_)
    return E_FAIL;

  if (!def_action)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(
      WebAccessibility::ATTR_SHORTCUT, def_action);
}

STDMETHODIMP BrowserAccessibility::get_accDescription(VARIANT var_id,
                                                      BSTR* desc) {
  if (!instance_active_)
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(WebAccessibility::ATTR_DESCRIPTION, desc);
}

STDMETHODIMP BrowserAccessibility::get_accFocus(VARIANT* focus_child) {
  if (!instance_active_)
    return E_FAIL;

  if (!focus_child)
    return E_INVALIDARG;

  BrowserAccessibility* focus = manager_->GetFocus(this);
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

STDMETHODIMP BrowserAccessibility::get_accHelp(VARIANT var_id, BSTR* help) {
  if (!instance_active_)
    return E_FAIL;

  if (!help)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(WebAccessibility::ATTR_HELP, help);
}

STDMETHODIMP BrowserAccessibility::get_accKeyboardShortcut(VARIANT var_id,
                                                           BSTR* acc_key) {
  if (!instance_active_)
    return E_FAIL;

  if (!acc_key)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetAttributeAsBstr(WebAccessibility::ATTR_SHORTCUT, acc_key);
}

STDMETHODIMP BrowserAccessibility::get_accName(VARIANT var_id, BSTR* name) {
  if (!instance_active_)
    return E_FAIL;

  if (!name)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (target->name_.empty())
    return S_FALSE;

  *name = SysAllocString(target->name_.c_str());

  DCHECK(*name);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accParent(IDispatch** disp_parent) {
  if (!instance_active_)
    return E_FAIL;

  if (!disp_parent)
    return E_INVALIDARG;

  IAccessible* parent = parent_;
  if (parent == NULL) {
    // This happens if we're the root of the tree;
    // return the IAccessible for the window.
    parent = manager_->GetParentWindowIAccessible();
  }

  parent->AddRef();
  *disp_parent = parent;
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accRole(VARIANT var_id, VARIANT* role) {
  if (!instance_active_)
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (!target->role_name_.empty()) {
    role->vt = VT_BSTR;
    role->bstrVal = SysAllocString(target->role_name_.c_str());
  } else {
    role->vt = VT_I4;
    role->lVal = target->role_;
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accState(VARIANT var_id,
                                                VARIANT* state) {
  if (!instance_active_)
    return E_FAIL;

  if (!state)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  state->vt = VT_I4;
  state->lVal = target->state_;
  if (manager_->GetFocus(NULL) == this)
    state->lVal |= STATE_SYSTEM_FOCUSED;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accValue(VARIANT var_id, BSTR* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  *value = SysAllocString(target->value_.c_str());

  DCHECK(*value);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accHelpTopic(BSTR* help_file,
                                                    VARIANT var_id,
                                                    LONG* topic_id) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::get_accSelection(VARIANT* selected) {
  if (!instance_active_)
    return E_FAIL;

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::accSelect(LONG flags_sel, VARIANT var_id) {
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

STDMETHODIMP BrowserAccessibility::role(LONG* role) {
  if (!instance_active_)
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  *role = ia2_role_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_attributes(BSTR* attributes) {
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

STDMETHODIMP BrowserAccessibility::get_states(AccessibleStates* states) {
  if (!instance_active_)
    return E_FAIL;

  if (!states)
    return E_INVALIDARG;

  *states = ia2_state_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_uniqueID(LONG* unique_id) {
  if (!instance_active_)
    return E_FAIL;

  if (!unique_id)
    return E_INVALIDARG;

  *unique_id = child_id_;
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_windowHandle(HWND* window_handle) {
  if (!instance_active_)
    return E_FAIL;

  if (!window_handle)
    return E_INVALIDARG;

  *window_handle = manager_->GetParentHWND();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_indexInParent(LONG* index_in_parent) {
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

STDMETHODIMP BrowserAccessibility::get_description(BSTR* desc) {
  if (!instance_active_)
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DESCRIPTION, desc);
}

STDMETHODIMP BrowserAccessibility::get_imagePosition(
    enum IA2CoordinateType coordinate_type, LONG* x, LONG* y) {
  if (!instance_active_)
    return E_FAIL;

  if (!x || !y)
    return E_INVALIDARG;

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    HWND parent_hwnd = manager_->GetParentHWND();
    POINT top_left = {0, 0};
    ::ClientToScreen(parent_hwnd, &top_left);
    *x = location_.x + top_left.x;
    *y = location_.y + top_left.y;
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    *x = location_.x;
    *y = location_.y;
    if (parent_) {
      *x -= parent_->location_.x;
      *y -= parent_->location_.y;
    }
  } else {
    return E_INVALIDARG;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_imageSize(LONG* height, LONG* width) {
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

STDMETHODIMP BrowserAccessibility::get_nCharacters(LONG* n_characters) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_characters)
    return E_INVALIDARG;

  if (src_role_ == WebAccessibility::ROLE_TEXT_FIELD) {
    *n_characters = value_.length();
  } else {
    *n_characters = name_.length();
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_text(
    LONG start_offset, LONG end_offset, BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  string16 text_str;
  if (src_role_ == WebAccessibility::ROLE_TEXT_FIELD) {
    text_str = value_;
  } else {
    text_str = name_;
  }

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

STDMETHODIMP BrowserAccessibility::get_caretOffset(LONG* offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  if (src_role_ == WebAccessibility::ROLE_TEXT_FIELD) {
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

STDMETHODIMP BrowserAccessibility::get_nSelections(LONG* n_selections) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_selections)
    return E_INVALIDARG;

  if (src_role_ == WebAccessibility::ROLE_TEXT_FIELD) {
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

STDMETHODIMP BrowserAccessibility::get_selection(LONG selection_index,
                                                 LONG* start_offset,
                                                 LONG* end_offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || selection_index != 0)
    return E_INVALIDARG;

  if (src_role_ == WebAccessibility::ROLE_TEXT_FIELD) {
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

//
// ISimpleDOMDocument methods.
//

STDMETHODIMP BrowserAccessibility::get_URL(BSTR* url) {
  if (!instance_active_)
    return E_FAIL;

  if (!url)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_URL, url);
}

STDMETHODIMP BrowserAccessibility::get_title(BSTR* title) {
  if (!instance_active_)
    return E_FAIL;

  if (!title)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_TITLE, title);
}

STDMETHODIMP BrowserAccessibility::get_mimeType(BSTR* mime_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!mime_type)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_MIMETYPE, mime_type);
}

STDMETHODIMP BrowserAccessibility::get_docType(BSTR* doc_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!doc_type)
    return E_INVALIDARG;

  return GetAttributeAsBstr(WebAccessibility::ATTR_DOC_DOCTYPE, doc_type);
}

//
// ISimpleDOMNode methods.
//

STDMETHODIMP BrowserAccessibility::get_nodeInfo(
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

  if (role_ == ROLE_SYSTEM_DOCUMENT) {
    *node_type = NODETYPE_DOCUMENT;
  } else if (role_ == ROLE_SYSTEM_TEXT &&
             ((ia2_state_ & IA2_STATE_EDITABLE) == 0)) {
    *node_type = NODETYPE_TEXT;
  } else {
    *node_type = NODETYPE_ELEMENT;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_attributes(
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

STDMETHODIMP BrowserAccessibility::get_attributesForNames(
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

STDMETHODIMP BrowserAccessibility::get_computedStyle(
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

STDMETHODIMP BrowserAccessibility::get_computedStyleForProperties(
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

STDMETHODIMP BrowserAccessibility::scrollTo(boolean placeTopLeft) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::get_parentNode(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  *node = parent_->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_firstChild(ISimpleDOMNode** node)  {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.size()) {
    *node = children_[0]->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibility::get_lastChild(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.size()) {
    *node = children_[children_.size() - 1]->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibility::get_previousSibling(
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (parent_ && index_in_parent_ > 0) {
    *node = parent_->children_[index_in_parent_ - 1]->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibility::get_nextSibling(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (parent_ &&
      index_in_parent_ >= 0 &&
      index_in_parent_ < static_cast<int>(parent_->children_.size()) - 1) {
    *node = parent_->children_[index_in_parent_ + 1]->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibility::get_childAt(
    unsigned int child_index,
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (child_index < children_.size()) {
    *node = children_[child_index]->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

//
// ISimpleDOMText methods.
//

STDMETHODIMP BrowserAccessibility::get_domText(BSTR* dom_text) {
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

STDMETHODIMP BrowserAccessibility::QueryService(
    REFGUID guidService, REFIID riid, void** object) {
  if (!instance_active_)
    return E_FAIL;

  if (guidService == IID_IAccessible ||
      guidService == IID_IAccessible2 ||
      guidService == IID_IAccessibleImage ||
      guidService == IID_IAccessibleText ||
      guidService == IID_ISimpleDOMDocument ||
      guidService == IID_ISimpleDOMNode ||
      guidService == IID_ISimpleDOMText) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

//
// CComObjectRootEx methods.
//

HRESULT WINAPI BrowserAccessibility::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID iid,
    void** object) {
  if (iid == IID_IAccessibleText) {
    if (role_ != ROLE_SYSTEM_LINK && role_ != ROLE_SYSTEM_TEXT) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleImage) {
    if (role_ != ROLE_SYSTEM_GRAPHIC) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_ISimpleDOMDocument) {
    if (role_ != ROLE_SYSTEM_DOCUMENT) {
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

BrowserAccessibility* BrowserAccessibility::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (var_id.vt != VT_I4)
    return NULL;

  LONG child_id = var_id.lVal;
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 && child_id <= static_cast<LONG>(children_.size()))
    return children_[child_id - 1];

  return manager_->GetFromChildID(child_id);
}

bool BrowserAccessibility::HasAttribute(WebAccessibility::Attribute attribute) {
  return (attributes_.find(attribute) != attributes_.end());
}

bool BrowserAccessibility::GetAttribute(
    WebAccessibility::Attribute attribute, string16* value) {
  std::map<int32, string16>::iterator iter = attributes_.find(attribute);
  if (iter != attributes_.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

HRESULT BrowserAccessibility::GetAttributeAsBstr(
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

bool BrowserAccessibility::GetAttributeAsInt(
    WebAccessibility::Attribute attribute, int* value_int) {
  string16 value_str;

  if (!GetAttribute(attribute, &value_str))
    return false;

  if (!base::StringToInt(value_str, value_int))
    return false;

  return true;
}

string16 BrowserAccessibility::Escape(string16 str) {
  return EscapeQueryParamValueUTF8(str, false);
}

void BrowserAccessibility::InitRoleAndState(LONG web_role, LONG web_state) {
  state_ = 0;
  ia2_state_ = IA2_STATE_OPAQUE;

  if ((web_state >> WebAccessibility::STATE_CHECKED) & 1)
    state_ |= STATE_SYSTEM_CHECKED;
  if ((web_state >> WebAccessibility::STATE_COLLAPSED) & 1)
    state_ |= STATE_SYSTEM_COLLAPSED;
  if ((web_state >> WebAccessibility::STATE_EXPANDED) & 1)
    state_ |= STATE_SYSTEM_EXPANDED;
  if ((web_state >> WebAccessibility::STATE_FOCUSABLE) & 1)
    state_ |= STATE_SYSTEM_FOCUSABLE;
  if ((web_state >> WebAccessibility::STATE_HASPOPUP) & 1)
    state_ |= STATE_SYSTEM_HASPOPUP;
  if ((web_state >> WebAccessibility::STATE_HOTTRACKED) & 1)
    state_ |= STATE_SYSTEM_HOTTRACKED;
  if ((web_state >> WebAccessibility::STATE_INDETERMINATE) & 1)
    state_ |= STATE_SYSTEM_INDETERMINATE;
  if ((web_state >> WebAccessibility::STATE_INVISIBLE) & 1)
    state_ |= STATE_SYSTEM_INVISIBLE;
  if ((web_state >> WebAccessibility::STATE_LINKED) & 1)
    state_ |= STATE_SYSTEM_LINKED;
  if ((web_state >> WebAccessibility::STATE_MULTISELECTABLE) & 1)
    state_ |= STATE_SYSTEM_MULTISELECTABLE;
  // TODO(ctguil): Support STATE_SYSTEM_EXTSELECTABLE/accSelect.
  if ((web_state >> WebAccessibility::STATE_OFFSCREEN) & 1)
    state_ |= STATE_SYSTEM_OFFSCREEN;
  if ((web_state >> WebAccessibility::STATE_PRESSED) & 1)
    state_ |= STATE_SYSTEM_PRESSED;
  if ((web_state >> WebAccessibility::STATE_PROTECTED) & 1)
    state_ |= STATE_SYSTEM_PROTECTED;
  if ((web_state >> WebAccessibility::STATE_SELECTABLE) & 1)
    state_ |= STATE_SYSTEM_SELECTABLE;
  if ((web_state >> WebAccessibility::STATE_SELECTED) & 1)
    state_ |= STATE_SYSTEM_SELECTED;
  if ((web_state >> WebAccessibility::STATE_READONLY) & 1)
    state_ |= STATE_SYSTEM_READONLY;
  if ((web_state >> WebAccessibility::STATE_TRAVERSED) & 1)
    state_ |= STATE_SYSTEM_TRAVERSED;
  if ((web_state >> WebAccessibility::STATE_BUSY) & 1)
    state_ |= STATE_SYSTEM_BUSY;
  if ((web_state >> WebAccessibility::STATE_UNAVAILABLE) & 1)
    state_ |= STATE_SYSTEM_UNAVAILABLE;

  role_ = 0;
  ia2_role_ = 0;
  switch (web_role) {
    case WebAccessibility::ROLE_ALERT:
    case WebAccessibility::ROLE_ALERT_DIALOG:
      role_ = ROLE_SYSTEM_ALERT;
      break;
    case WebAccessibility::ROLE_APPLICATION:
      role_ = ROLE_SYSTEM_APPLICATION;
      break;
    case WebAccessibility::ROLE_ARTICLE:
      role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_BUTTON:
      role_ = ROLE_SYSTEM_PUSHBUTTON;
      break;
    case WebAccessibility::ROLE_CELL:
      role_ = ROLE_SYSTEM_CELL;
      break;
    case WebAccessibility::ROLE_CHECKBOX:
      role_ = ROLE_SYSTEM_CHECKBUTTON;
      break;
    case WebAccessibility::ROLE_COLOR_WELL:
      role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_COLOR_CHOOSER;
      break;
    case WebAccessibility::ROLE_COLUMN:
      role_ = ROLE_SYSTEM_COLUMN;
      break;
    case WebAccessibility::ROLE_COLUMN_HEADER:
      role_ = ROLE_SYSTEM_COLUMNHEADER;
      break;
    case WebAccessibility::ROLE_COMBO_BOX:
      role_ = ROLE_SYSTEM_COMBOBOX;
      break;
    case WebAccessibility::ROLE_DEFINITION_LIST_DEFINITION:
      GetAttribute(WebAccessibility::ATTR_HTML_TAG, &role_name_);
      ia2_role_ = IA2_ROLE_PARAGRAPH;
      break;
    case WebAccessibility::ROLE_DEFINITION_LIST_TERM:
      role_ = ROLE_SYSTEM_LISTITEM;
      break;
    case WebAccessibility::ROLE_DIALOG:
      role_ = ROLE_SYSTEM_DIALOG;
      break;
    case WebAccessibility::ROLE_DOCUMENT:
    case WebAccessibility::ROLE_WEB_AREA:
      role_ = ROLE_SYSTEM_DOCUMENT;
      state_ |= STATE_SYSTEM_READONLY;
      state_ |= STATE_SYSTEM_FOCUSABLE;
      break;
    case WebAccessibility::ROLE_EDITABLE_TEXT:
      role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case WebAccessibility::ROLE_GRID:
      role_ = ROLE_SYSTEM_TABLE;
      break;
    case WebAccessibility::ROLE_GROUP:
      GetAttribute(WebAccessibility::ATTR_HTML_TAG, &role_name_);
      if (role_name_.empty())
        role_name_ = L"div";
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_HEADING:
      GetAttribute(WebAccessibility::ATTR_HTML_TAG, &role_name_);
      ia2_role_ = IA2_ROLE_HEADING;
      break;
    case WebAccessibility::ROLE_IMAGE:
      role_ = ROLE_SYSTEM_GRAPHIC;
      break;
    case WebAccessibility::ROLE_IMAGE_MAP:
      GetAttribute(WebAccessibility::ATTR_HTML_TAG, &role_name_);
      ia2_role_ = IA2_ROLE_IMAGE_MAP;
      break;
    case WebAccessibility::ROLE_IMAGE_MAP_LINK:
      role_ = ROLE_SYSTEM_LINK;
      state_ |= STATE_SYSTEM_LINKED;
      break;
    case WebAccessibility::ROLE_LANDMARK_APPLICATION:
    case WebAccessibility::ROLE_LANDMARK_BANNER:
    case WebAccessibility::ROLE_LANDMARK_COMPLEMENTARY:
    case WebAccessibility::ROLE_LANDMARK_CONTENTINFO:
    case WebAccessibility::ROLE_LANDMARK_MAIN:
    case WebAccessibility::ROLE_LANDMARK_NAVIGATION:
    case WebAccessibility::ROLE_LANDMARK_SEARCH:
      role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_LINK:
    case WebAccessibility::ROLE_WEBCORE_LINK:
      role_ = ROLE_SYSTEM_LINK;
      state_ |= STATE_SYSTEM_LINKED;
      break;
    case WebAccessibility::ROLE_LIST:
      role_ = ROLE_SYSTEM_LIST;
      break;
    case WebAccessibility::ROLE_LISTBOX:
      role_ = ROLE_SYSTEM_LIST;
      break;
    case WebAccessibility::ROLE_LISTBOX_OPTION:
    case WebAccessibility::ROLE_LIST_ITEM:
    case WebAccessibility::ROLE_LIST_MARKER:
      role_ = ROLE_SYSTEM_LISTITEM;
      break;
    case WebAccessibility::ROLE_MENU:
    case WebAccessibility::ROLE_MENU_BUTTON:
      role_ = ROLE_SYSTEM_MENUPOPUP;
      break;
    case WebAccessibility::ROLE_MENU_BAR:
      role_ = ROLE_SYSTEM_MENUBAR;
      break;
    case WebAccessibility::ROLE_MENU_ITEM:
    case WebAccessibility::ROLE_MENU_LIST_OPTION:
      role_ = ROLE_SYSTEM_MENUITEM;
      break;
    case WebAccessibility::ROLE_MENU_LIST_POPUP:
      role_ = ROLE_SYSTEM_MENUPOPUP;
      break;
    case WebAccessibility::ROLE_NOTE:
      role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_NOTE;
      break;
    case WebAccessibility::ROLE_OUTLINE:
      role_ = ROLE_SYSTEM_OUTLINE;
      break;
    case WebAccessibility::ROLE_POPUP_BUTTON:
      role_ = ROLE_SYSTEM_COMBOBOX;
      break;
    case WebAccessibility::ROLE_PROGRESS_INDICATOR:
      role_ = ROLE_SYSTEM_PROGRESSBAR;
      break;
    case WebAccessibility::ROLE_RADIO_BUTTON:
      role_ = ROLE_SYSTEM_RADIOBUTTON;
      break;
    case WebAccessibility::ROLE_RADIO_GROUP:
      role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_REGION:
      role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_ROW:
      role_ = ROLE_SYSTEM_ROW;
      break;
    case WebAccessibility::ROLE_ROW_HEADER:
      role_ = ROLE_SYSTEM_ROWHEADER;
      break;
    case WebAccessibility::ROLE_RULER:
      role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_RULER;
      break;
    case WebAccessibility::ROLE_SCROLLAREA:
      role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SCROLL_PANE;
      break;
    case WebAccessibility::ROLE_SCROLLBAR:
      role_ = ROLE_SYSTEM_SCROLLBAR;
      break;
    case WebAccessibility::ROLE_SLIDER:
      role_ = ROLE_SYSTEM_SLIDER;
      break;
    case WebAccessibility::ROLE_SPLIT_GROUP:
      role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SPLIT_PANE;
      break;
    case WebAccessibility::ROLE_ANNOTATION:
    case WebAccessibility::ROLE_STATIC_TEXT:
      role_ = ROLE_SYSTEM_TEXT;
      break;
    case WebAccessibility::ROLE_STATUS:
      role_ = ROLE_SYSTEM_STATUSBAR;
      break;
    case WebAccessibility::ROLE_TAB:
      role_ = ROLE_SYSTEM_PAGETAB;
      break;
    case WebAccessibility::ROLE_TABLE:
      role_ = ROLE_SYSTEM_TABLE;
      break;
    case WebAccessibility::ROLE_TABLE_HEADER_CONTAINER:
      role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_TAB_GROUP:
    case WebAccessibility::ROLE_TAB_LIST:
    case WebAccessibility::ROLE_TAB_PANEL:
      role_ = ROLE_SYSTEM_PAGETABLIST;
      break;
    case WebAccessibility::ROLE_TEXTAREA:
      role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_MULTI_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case WebAccessibility::ROLE_TEXT_FIELD:
      role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case WebAccessibility::ROLE_TOOLBAR:
      role_ = ROLE_SYSTEM_TOOLBAR;
      break;
    case WebAccessibility::ROLE_TOOLTIP:
      role_ = ROLE_SYSTEM_TOOLTIP;
      break;
    case WebAccessibility::ROLE_TREE:
      role_ = ROLE_SYSTEM_OUTLINE;
      break;
    case WebAccessibility::ROLE_TREE_GRID:
      role_ = ROLE_SYSTEM_OUTLINE;
      break;
    case WebAccessibility::ROLE_TREE_ITEM:
      role_ = ROLE_SYSTEM_OUTLINEITEM;
      break;
    case WebAccessibility::ROLE_WINDOW:
      role_ = ROLE_SYSTEM_WINDOW;
      break;

    // TODO(dmazzoni): figure out the proper MSAA role for all of these.
    case WebAccessibility::ROLE_BROWSER:
    case WebAccessibility::ROLE_BUSY_INDICATOR:
    case WebAccessibility::ROLE_DIRECTORY:
    case WebAccessibility::ROLE_DISCLOSURE_TRIANGLE:
    case WebAccessibility::ROLE_DRAWER:
    case WebAccessibility::ROLE_GROW_AREA:
    case WebAccessibility::ROLE_HELP_TAG:
    case WebAccessibility::ROLE_IGNORED:
    case WebAccessibility::ROLE_INCREMENTOR:
    case WebAccessibility::ROLE_LOG:
    case WebAccessibility::ROLE_MARQUEE:
    case WebAccessibility::ROLE_MATH:
    case WebAccessibility::ROLE_MATTE:
    case WebAccessibility::ROLE_RULER_MARKER:
    case WebAccessibility::ROLE_SHEET:
    case WebAccessibility::ROLE_SLIDER_THUMB:
    case WebAccessibility::ROLE_SPLITTER:
    case WebAccessibility::ROLE_SYSTEM_WIDE:
    case WebAccessibility::ROLE_TIMER:
    case WebAccessibility::ROLE_VALUE_INDICATOR:
    default:
      role_ = ROLE_SYSTEM_CLIENT;
      break;
  }

  // The role should always be set.
  DCHECK(!role_name_.empty() || role_);

  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!ia2_role_)
    ia2_role_ = role_;
}
