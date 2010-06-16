// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility_win.h"

#include "base/logging.h"
#include "chrome/browser/browser_accessibility_manager_win.h"

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
  location_ = src.location;
  InitRoleAndState(src.role, src.state);

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
}

bool BrowserAccessibility::IsDescendantOf(BrowserAccessibility* ancestor) {
  if (this == ancestor) {
    return true;
  } else if (parent_) {
    return parent_->IsDescendantOf(ancestor);
  }

  return false;
}

BrowserAccessibility* BrowserAccessibility::GetPreviousSibling() {
  if (parent_ && index_in_parent_ > 0)
    return parent_->children_[index_in_parent_ - 1];

  return NULL;
}

BrowserAccessibility* BrowserAccessibility::GetNextSibling() {
  if (parent_ &&
      index_in_parent_ < static_cast<int>(parent_->children_.size() - 1)) {
    return parent_->children_[index_in_parent_ + 1];
  }

  return NULL;
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

  string16 action;
  if (!target->GetAttribute(WebAccessibility::ATTR_SHORTCUT, &action))
    return S_FALSE;

  if (action.empty())
    return S_FALSE;

  *def_action = SysAllocString(action.c_str());

  DCHECK(*def_action);
  return S_OK;
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

  string16 description;
  if (!target->GetAttribute(WebAccessibility::ATTR_DESCRIPTION, &description))
    return S_FALSE;

  if (description.empty())
    return S_FALSE;

  *desc = SysAllocString(description.c_str());

  DCHECK(*desc);
  return S_OK;
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

  string16 help_str;
  if (!target->GetAttribute(WebAccessibility::ATTR_HELP, &help_str))
    return S_FALSE;

  if (help_str.empty())
    return S_FALSE;

  *help = SysAllocString(help_str.c_str());

  DCHECK(*help);
  return S_OK;
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

  string16 shortcut;
  if (!target->GetAttribute(WebAccessibility::ATTR_SHORTCUT, &shortcut))
    return S_FALSE;

  if (shortcut.empty())
    return S_FALSE;

  *acc_key = SysAllocString(shortcut.c_str());

  DCHECK(*acc_key);
  return S_OK;
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

  return S_FALSE;
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

  string16 description;
  if (!GetAttribute(WebAccessibility::ATTR_DESCRIPTION, &description))
    return S_FALSE;

  if (description.empty())
    return S_FALSE;

  *desc = SysAllocString(description.c_str());

  DCHECK(*desc);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_imagePosition(
    enum IA2CoordinateType coordinate_type, long* x, long* y) {
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

STDMETHODIMP BrowserAccessibility::get_imageSize(long* height, long* width) {
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

STDMETHODIMP BrowserAccessibility::get_nCharacters(long* n_characters) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_characters)
    return E_INVALIDARG;

  *n_characters = name_.length();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_text(
    long start_offset, long end_offset, BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  long len = name_.length();
  if (start_offset < 0)
    start_offset = 0;
  if (end_offset > len)
    end_offset = len;

  *text = SysAllocString(
      name_.substr(start_offset, end_offset - start_offset).c_str());
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_caretOffset(long* offset) {
  *offset = 0;
  return S_OK;
}

//
// IServiceProvider methods.
//

STDMETHODIMP BrowserAccessibility::QueryService(
    REFGUID guidService, REFIID riid, void** object) {
  if (!instance_active_)
    return E_FAIL;

  if (guidService == IID_IAccessible || guidService == IID_IAccessible2)
    return QueryInterface(riid, object);

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
    if (role_ != ROLE_SYSTEM_LINK) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleImage) {
    if (role_ != ROLE_SYSTEM_GRAPHIC) {
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

void BrowserAccessibility::InitRoleAndState(LONG web_role,
                                            LONG web_state) {
  state_ = 0;
  ia2_state_ = IA2_STATE_OPAQUE;

  if ((web_state >> WebAccessibility::STATE_CHECKED) & 1)
    state_ |= STATE_SYSTEM_CHECKED;
  if ((web_state >> WebAccessibility::STATE_FOCUSABLE) & 1)
    state_ |= STATE_SYSTEM_FOCUSABLE;
  if ((web_state >> WebAccessibility::STATE_HOTTRACKED) & 1)
    state_ |= STATE_SYSTEM_HOTTRACKED;
  if ((web_state >> WebAccessibility::STATE_INDETERMINATE) & 1)
    state_ |= STATE_SYSTEM_INDETERMINATE;
  if ((web_state >> WebAccessibility::STATE_LINKED) & 1)
    state_ |= STATE_SYSTEM_LINKED;
  if ((web_state >> WebAccessibility::STATE_MULTISELECTABLE) & 1)
    state_ |= STATE_SYSTEM_MULTISELECTABLE;
  if ((web_state >> WebAccessibility::STATE_OFFSCREEN) & 1)
    state_ |= STATE_SYSTEM_OFFSCREEN;
  if ((web_state >> WebAccessibility::STATE_PRESSED) & 1)
    state_ |= STATE_SYSTEM_PRESSED;
  if ((web_state >> WebAccessibility::STATE_PROTECTED) & 1)
    state_ |= STATE_SYSTEM_PROTECTED;
  if ((web_state >> WebAccessibility::STATE_READONLY) & 1)
    state_ |= STATE_SYSTEM_READONLY;
  if ((web_state >> WebAccessibility::STATE_TRAVERSED) & 1)
    state_ |= STATE_SYSTEM_TRAVERSED;
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
      role_name_ = L"dd";
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
      role_name_ = L"div";
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_HEADING:
      // TODO(dmazzoni): support all heading levels
      role_name_ = L"h1";
      ia2_role_ = IA2_ROLE_HEADING;
      break;
    case WebAccessibility::ROLE_IMAGE:
      role_ = ROLE_SYSTEM_GRAPHIC;
      break;
    case WebAccessibility::ROLE_IMAGE_MAP:
      role_name_ = L"map";
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
