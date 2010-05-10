// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility.h"

#include "base/logging.h"
#include "chrome/browser/browser_accessibility_manager.h"

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
  action_ = src.action;
  description_ = src.description;
  help_ = src.help;
  shortcut_ = src.shortcut;
  role_ = MSAARole(src.role);
  state_ = MSAAState(src.state);
  location_ = src.location;

  instance_active_ = true;

  // Focused is a dynamic state, only one node will be focused at a time.
  state_ &= ~STATE_SYSTEM_FOCUSED;
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

HRESULT BrowserAccessibility::accDoDefaultAction(VARIANT var_id) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::accHitTest(LONG x_left, LONG y_top,
                                              VARIANT* child) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!child)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::accLocation(LONG* x_left, LONG* y_top,
                                               LONG* width, LONG* height,
                                               VARIANT var_id) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

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

  BrowserAccessibility* result;
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
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

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
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!child_count)
    return E_INVALIDARG;

  *child_count = children_.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accDefaultAction(VARIANT var_id,
                                                        BSTR* def_action) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!def_action)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  // Return false if the string is empty.
  if (target->action_.size() == 0)
    return S_FALSE;

  *def_action = SysAllocString(target->action_.c_str());

  DCHECK(*def_action);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accDescription(VARIANT var_id,
                                                      BSTR* desc) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!desc)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  // Return false if the string is empty.
  if (target->description_.size() == 0)
    return S_FALSE;

  *desc = SysAllocString(target->description_.c_str());

  DCHECK(*desc);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accFocus(VARIANT* focus_child) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

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
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!help)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  // Return false if the string is empty.
  if (target->help_.size() == 0)
    return S_FALSE;

  *help = SysAllocString(target->help_.c_str());

  DCHECK(*help);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accKeyboardShortcut(VARIANT var_id,
                                                           BSTR* acc_key) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!acc_key)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  // Return false if the string is empty.
  if (target->shortcut_.size() == 0)
    return S_FALSE;

  *acc_key = SysAllocString(target->shortcut_.c_str());

  DCHECK(*acc_key);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accName(VARIANT var_id, BSTR* name) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!name)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  // Return false if the string is empty.
  if (target->name_.size() == 0)
    return S_FALSE;

  *name = SysAllocString(target->name_.c_str());

  DCHECK(*name);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accParent(IDispatch** disp_parent) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

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
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!role)
    return E_INVALIDARG;

  BrowserAccessibility* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  role->vt = VT_I4;
  role->lVal = target->role_;
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accState(VARIANT var_id,
                                                VARIANT* state) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

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
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

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
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::accSelect(LONG flags_sel, VARIANT var_id) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  return E_NOTIMPL;
}

//
// IAccessible2 methods.
//

STDMETHODIMP BrowserAccessibility::role(LONG* role) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!role)
    return E_INVALIDARG;

  *role = role_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_states(AccessibleStates* states) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!states)
    return E_INVALIDARG;

  *states = state_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_uniqueID(LONG* unique_id) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!unique_id)
    return E_INVALIDARG;

  *unique_id = child_id_;
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_windowHandle(HWND* window_handle) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!window_handle)
    return E_INVALIDARG;

  *window_handle = manager_->GetParentHWND();
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_indexInParent(LONG* index_in_parent) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!index_in_parent)
    return E_INVALIDARG;

  *index_in_parent = index_in_parent_;
  return S_OK;
}

//
// IServiceProvider methods.
//

STDMETHODIMP BrowserAccessibility::QueryService(
    REFGUID guidService, REFIID riid, void** object) {
  if (!instance_active_) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (guidService == IID_IAccessible || guidService == IID_IAccessible2)
    return QueryInterface(riid, object);

  *object = NULL;
  return E_FAIL;
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

LONG BrowserAccessibility::MSAARole(LONG browser_accessibility_role) {
  switch (browser_accessibility_role) {
    case WebAccessibility::ROLE_APPLICATION:
      return ROLE_SYSTEM_APPLICATION;
    case WebAccessibility::ROLE_CELL:
      return ROLE_SYSTEM_CELL;
    case WebAccessibility::ROLE_CHECKBUTTON:
      return ROLE_SYSTEM_CHECKBUTTON;
    case WebAccessibility::ROLE_COLUMN:
      return ROLE_SYSTEM_COLUMN;
    case WebAccessibility::ROLE_COLUMNHEADER:
      return ROLE_SYSTEM_COLUMNHEADER;
    case WebAccessibility::ROLE_DOCUMENT:
      return ROLE_SYSTEM_DOCUMENT;
    case WebAccessibility::ROLE_GRAPHIC:
      return ROLE_SYSTEM_GRAPHIC;
    case WebAccessibility::ROLE_GROUPING:
      return ROLE_SYSTEM_GROUPING;
    case WebAccessibility::ROLE_LINK:
      return ROLE_SYSTEM_LINK;
    case WebAccessibility::ROLE_LIST:
    case WebAccessibility::ROLE_LISTBOX:
      return ROLE_SYSTEM_LIST;
    case WebAccessibility::ROLE_LISTITEM:
      return ROLE_SYSTEM_LISTITEM;
    case WebAccessibility::ROLE_MENUBAR:
      return ROLE_SYSTEM_MENUBAR;
    case WebAccessibility::ROLE_MENUITEM:
      return ROLE_SYSTEM_MENUITEM;
    case WebAccessibility::ROLE_MENUPOPUP:
      return ROLE_SYSTEM_MENUPOPUP;
    case WebAccessibility::ROLE_OUTLINE:
      return ROLE_SYSTEM_OUTLINE;
    case WebAccessibility::ROLE_PAGETABLIST:
      return ROLE_SYSTEM_PAGETABLIST;
    case WebAccessibility::ROLE_PROGRESSBAR:
      return ROLE_SYSTEM_PROGRESSBAR;
    case WebAccessibility::ROLE_PUSHBUTTON:
      return ROLE_SYSTEM_PUSHBUTTON;
    case WebAccessibility::ROLE_RADIOBUTTON:
      return ROLE_SYSTEM_RADIOBUTTON;
    case WebAccessibility::ROLE_ROW:
      return ROLE_SYSTEM_ROW;
    case WebAccessibility::ROLE_ROWHEADER:
      return ROLE_SYSTEM_ROWHEADER;
    case WebAccessibility::ROLE_SEPARATOR:
      return ROLE_SYSTEM_SEPARATOR;
    case WebAccessibility::ROLE_SLIDER:
      return ROLE_SYSTEM_SLIDER;
    case WebAccessibility::ROLE_STATICTEXT:
      return ROLE_SYSTEM_STATICTEXT;
    case WebAccessibility::ROLE_STATUSBAR:
      return ROLE_SYSTEM_STATUSBAR;
    case WebAccessibility::ROLE_TABLE:
      return ROLE_SYSTEM_TABLE;
    case WebAccessibility::ROLE_TEXT:
      return ROLE_SYSTEM_TEXT;
    case WebAccessibility::ROLE_TOOLBAR:
      return ROLE_SYSTEM_TOOLBAR;
    case WebAccessibility::ROLE_TOOLTIP:
      return ROLE_SYSTEM_TOOLTIP;
    case WebAccessibility::ROLE_CLIENT:
    default:
      // This is the default role for MSAA.
      return ROLE_SYSTEM_CLIENT;
  }
}

LONG BrowserAccessibility::MSAAState(LONG browser_accessibility_state) {
  LONG state = 0;

  if ((browser_accessibility_state >> WebAccessibility::STATE_CHECKED) & 1)
    state |= STATE_SYSTEM_CHECKED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_FOCUSABLE) & 1)
    state |= STATE_SYSTEM_FOCUSABLE;

  if ((browser_accessibility_state >> WebAccessibility::STATE_FOCUSED) & 1)
    state |= STATE_SYSTEM_FOCUSED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_HOTTRACKED) & 1)
    state |= STATE_SYSTEM_HOTTRACKED;

  if ((browser_accessibility_state >>
       WebAccessibility::STATE_INDETERMINATE) & 1) {
    state |= STATE_SYSTEM_INDETERMINATE;
  }

  if ((browser_accessibility_state >> WebAccessibility::STATE_LINKED) & 1)
    state |= STATE_SYSTEM_LINKED;

  if ((browser_accessibility_state >>
       WebAccessibility::STATE_MULTISELECTABLE) & 1) {
    state |= STATE_SYSTEM_MULTISELECTABLE;
  }

  if ((browser_accessibility_state >> WebAccessibility::STATE_OFFSCREEN) & 1)
    state |= STATE_SYSTEM_OFFSCREEN;

  if ((browser_accessibility_state >> WebAccessibility::STATE_PRESSED) & 1)
    state |= STATE_SYSTEM_PRESSED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_PROTECTED) & 1)
    state |= STATE_SYSTEM_PROTECTED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_READONLY) & 1)
    state |= STATE_SYSTEM_READONLY;

  if ((browser_accessibility_state >> WebAccessibility::STATE_TRAVERSED) & 1)
    state |= STATE_SYSTEM_TRAVERSED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_UNAVAILABLE) & 1)
    state |= STATE_SYSTEM_UNAVAILABLE;

  return state;
}
