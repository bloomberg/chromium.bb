// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_win.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/enum_variant.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/common/accessibility_messages.h"
#include "ui/base/accessibility/accessible_text_utils.h"
#include "ui/base/win/accessibility_misc_utils.h"

namespace content {

// These nonstandard GUIDs are taken directly from the Mozilla sources
// (accessible/src/msaa/nsAccessNodeWrap.cpp); some documentation is here:
// http://developer.mozilla.org/en/Accessibility/AT-APIs/ImplementationFeatures/MSAA
const GUID GUID_ISimpleDOM = {
    0x0c539790, 0x12e4, 0x11cf,
    0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8};
const GUID GUID_IAccessibleContentDocument = {
    0xa5d8e1f3, 0x3571, 0x4d8f,
    0x95, 0x21, 0x07, 0xed, 0x28, 0xfb, 0x07, 0x2e};

const char16 BrowserAccessibilityWin::kEmbeddedCharacter[] = L"\xfffc";

//
// BrowserAccessibilityRelation
//
// A simple implementation of IAccessibleRelation, used to represent
// a relationship between two accessible nodes in the tree.
//

class BrowserAccessibilityRelation
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IAccessibleRelation {
  BEGIN_COM_MAP(BrowserAccessibilityRelation)
    COM_INTERFACE_ENTRY(IAccessibleRelation)
  END_COM_MAP()

  CONTENT_EXPORT BrowserAccessibilityRelation() {}
  CONTENT_EXPORT virtual ~BrowserAccessibilityRelation() {}

  CONTENT_EXPORT void Initialize(BrowserAccessibilityWin* owner,
                                 const string16& type);
  CONTENT_EXPORT void AddTarget(int target_id);

  // IAccessibleRelation methods.
  CONTENT_EXPORT STDMETHODIMP get_relationType(BSTR* relation_type);
  CONTENT_EXPORT STDMETHODIMP get_nTargets(long* n_targets);
  CONTENT_EXPORT STDMETHODIMP get_target(long target_index, IUnknown** target);
  CONTENT_EXPORT STDMETHODIMP get_targets(long max_targets,
                                          IUnknown** targets,
                                          long* n_targets);

  // IAccessibleRelation methods not implemented.
  CONTENT_EXPORT STDMETHODIMP get_localizedRelationType(BSTR* relation_type) {
    return E_NOTIMPL;
  }

 private:
  string16 type_;
  base::win::ScopedComPtr<BrowserAccessibilityWin> owner_;
  std::vector<int> target_ids_;
};

void BrowserAccessibilityRelation::Initialize(BrowserAccessibilityWin* owner,
                                              const string16& type) {
  owner_ = owner;
  type_ = type;
}

void BrowserAccessibilityRelation::AddTarget(int target_id) {
  target_ids_.push_back(target_id);
}

STDMETHODIMP BrowserAccessibilityRelation::get_relationType(
    BSTR* relation_type) {
  if (!relation_type)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  *relation_type = SysAllocString(type_.c_str());
  DCHECK(*relation_type);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityRelation::get_nTargets(long* n_targets) {
  if (!n_targets)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  *n_targets = static_cast<long>(target_ids_.size());

  BrowserAccessibilityManager* manager = owner_->manager();
  for (long i = *n_targets - 1; i >= 0; --i) {
    BrowserAccessibility* result = manager->GetFromRendererID(target_ids_[i]);
    if (!result || !result->instance_active()) {
      *n_targets = 0;
      break;
    }
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityRelation::get_target(long target_index,
                                                      IUnknown** target) {
  if (!target)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  if (target_index < 0 ||
      target_index >= static_cast<long>(target_ids_.size())) {
    return E_INVALIDARG;
  }

  BrowserAccessibilityManager* manager = owner_->manager();
  BrowserAccessibility* result =
      manager->GetFromRendererID(target_ids_[target_index]);
  if (!result || !result->instance_active())
    return E_FAIL;

  *target = static_cast<IAccessible*>(
      result->ToBrowserAccessibilityWin()->NewReference());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityRelation::get_targets(long max_targets,
                                                       IUnknown** targets,
                                                       long* n_targets) {
  if (!targets || !n_targets)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  long count = static_cast<long>(target_ids_.size());
  if (count > max_targets)
    count = max_targets;

  *n_targets = count;
  if (count == 0)
    return S_FALSE;

  for (long i = 0; i < count; ++i) {
    HRESULT result = get_target(i, &targets[i]);
    if (result != S_OK)
      return result;
  }

  return S_OK;
}

//
// BrowserAccessibilityWin
//

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  CComObject<BrowserAccessibilityWin>* instance;
  HRESULT hr = CComObject<BrowserAccessibilityWin>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  return instance->NewReference();
}

BrowserAccessibilityWin* BrowserAccessibility::ToBrowserAccessibilityWin() {
  return static_cast<BrowserAccessibilityWin*>(this);
}

BrowserAccessibilityWin::BrowserAccessibilityWin()
    : ia_role_(0),
      ia_state_(0),
      ia2_role_(0),
      ia2_state_(0),
      first_time_(true),
      old_ia_state_(0) {
}

BrowserAccessibilityWin::~BrowserAccessibilityWin() {
  for (size_t i = 0; i < relations_.size(); ++i)
    relations_[i]->Release();
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
  if (!GetGlobalBoundsRect().Contains(point)) {
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
    child->pdispVal = result->ToBrowserAccessibilityWin()->NewReference();
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accLocation(LONG* x_left,
                                                  LONG* y_top,
                                                  LONG* width,
                                                  LONG* height,
                                                  VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  if (!x_left || !y_top || !width || !height)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  gfx::Rect bounds = target->GetGlobalBoundsRect();
  *x_left = bounds.x();
  *y_top  = bounds.y();
  *width  = bounds.width();
  *height = bounds.height();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accNavigate(LONG nav_dir,
                                                  VARIANT start,
                                                  VARIANT* end) {
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
  end->pdispVal = result->ToBrowserAccessibilityWin()->NewReference();
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

  return target->GetStringAttributeAsBstr(
      AccessibilityNodeData::ATTR_SHORTCUT, def_action);
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

  return target->GetStringAttributeAsBstr(
      AccessibilityNodeData::ATTR_DESCRIPTION, desc);
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

  return target->GetStringAttributeAsBstr(
      AccessibilityNodeData::ATTR_HELP, help);
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

  return target->GetStringAttributeAsBstr(
      AccessibilityNodeData::ATTR_SHORTCUT, acc_key);
}

STDMETHODIMP BrowserAccessibilityWin::get_accName(VARIANT var_id, BSTR* name) {
  if (!instance_active_)
    return E_FAIL;

  if (!name)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  string16 name_str = target->name_;

  // If the name is empty, see if it's labeled by another element.
  if (name_str.empty()) {
    int title_elem_id;
    if (target->GetIntAttribute(AccessibilityNodeData::ATTR_TITLE_UI_ELEMENT,
                                &title_elem_id)) {
      BrowserAccessibility* title_elem =
          manager_->GetFromRendererID(title_elem_id);
      if (title_elem)
        name_str = title_elem->GetTextRecursive();
    }
  }

  if (name_str.empty())
    return S_FALSE;

  *name = SysAllocString(name_str.c_str());

  DCHECK(*name);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accParent(IDispatch** disp_parent) {
  if (!instance_active_)
    return E_FAIL;

  if (!disp_parent)
    return E_INVALIDARG;

  IAccessible* parent = parent_->ToBrowserAccessibilityWin();
  if (parent == NULL) {
    // This happens if we're the root of the tree;
    // return the IAccessible for the window.
    parent = manager_->ToBrowserAccessibilityManagerWin()->
             GetParentWindowIAccessible();
  }

  parent->AddRef();
  *disp_parent = parent;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accRole(VARIANT var_id,
                                                  VARIANT* role) {
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

STDMETHODIMP BrowserAccessibilityWin::get_accValue(VARIANT var_id,
                                                   BSTR* value) {
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

STDMETHODIMP BrowserAccessibilityWin::get_accHelpTopic(BSTR* help_file,
                                                       VARIANT var_id,
                                                       LONG* topic_id) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_accSelection(VARIANT* selected) {
  if (!instance_active_)
    return E_FAIL;

  if (role_ != AccessibilityNodeData::ROLE_LISTBOX)
    return E_NOTIMPL;

  unsigned long selected_count = 0;
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->HasState(AccessibilityNodeData::STATE_SELECTED))
      ++selected_count;
  }

  if (selected_count == 0) {
    selected->vt = VT_EMPTY;
    return S_OK;
  }

  if (selected_count == 1) {
    for (size_t i = 0; i < children_.size(); ++i) {
      if (children_[i]->HasState(AccessibilityNodeData::STATE_SELECTED)) {
        selected->vt = VT_DISPATCH;
        selected->pdispVal =
            children_[i]->ToBrowserAccessibilityWin()->NewReference();
        return S_OK;
      }
    }
  }

  // Multiple items are selected.
  base::win::EnumVariant* enum_variant =
      new base::win::EnumVariant(selected_count);
  enum_variant->AddRef();
  unsigned long index = 0;
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->HasState(AccessibilityNodeData::STATE_SELECTED)) {
      enum_variant->ItemAt(index)->vt = VT_DISPATCH;
      enum_variant->ItemAt(index)->pdispVal =
        children_[i]->ToBrowserAccessibilityWin()->NewReference();
      ++index;
    }
  }
  selected->vt = VT_UNKNOWN;
  selected->punkVal = static_cast<IUnknown*>(
      static_cast<base::win::IUnknownImpl*>(enum_variant));
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accSelect(
    LONG flags_sel, VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  if (flags_sel & SELFLAG_TAKEFOCUS) {
    manager_->SetFocus(this, true);
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

  // The iaccessible2 attributes are a set of key-value pairs
  // separated by semicolons, with a colon between the key and the value.
  string16 str;
  for (unsigned int i = 0; i < ia2_attributes_.size(); ++i) {
    if (i != 0)
      str += L';';
    str += ia2_attributes_[i];
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

STDMETHODIMP BrowserAccessibilityWin::get_nRelations(LONG* n_relations) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_relations)
    return E_INVALIDARG;

  *n_relations = relations_.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_relation(
    LONG relation_index,
    IAccessibleRelation** relation) {
  if (!instance_active_)
    return E_FAIL;

  if (relation_index < 0 ||
      relation_index >= static_cast<long>(relations_.size())) {
    return E_INVALIDARG;
  }

  if (!relation)
    return E_INVALIDARG;

  relations_[relation_index]->AddRef();
  *relation = relations_[relation_index];
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_relations(
    LONG max_relations,
    IAccessibleRelation** relations,
    LONG* n_relations) {
  if (!instance_active_)
    return E_FAIL;

  if (!relations || !n_relations)
    return E_INVALIDARG;

  long count = static_cast<long>(relations_.size());
  *n_relations = count;
  if (count == 0)
    return S_FALSE;

  for (long i = 0; i < count; ++i) {
    relations_[i]->AddRef();
    relations[i] = relations_[i];
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollTo(enum IA2ScrollType scroll_type) {
  if (!instance_active_)
    return E_FAIL;

  gfx::Rect r = location_;
  switch(scroll_type) {
    case IA2_SCROLL_TYPE_TOP_LEFT:
      manager_->ScrollToMakeVisible(*this, gfx::Rect(r.x(), r.y(), 0, 0));
      break;
    case IA2_SCROLL_TYPE_BOTTOM_RIGHT:
      manager_->ScrollToMakeVisible(
          *this, gfx::Rect(r.right(), r.bottom(), 0, 0));
      break;
    case IA2_SCROLL_TYPE_TOP_EDGE:
      manager_->ScrollToMakeVisible(
          *this, gfx::Rect(r.x(), r.y(), r.width(), 0));
      break;
    case IA2_SCROLL_TYPE_BOTTOM_EDGE:
      manager_->ScrollToMakeVisible(
          *this, gfx::Rect(r.x(), r.bottom(), r.width(), 0));
    break;
    case IA2_SCROLL_TYPE_LEFT_EDGE:
      manager_->ScrollToMakeVisible(
          *this, gfx::Rect(r.x(), r.y(), 0, r.height()));
      break;
    case IA2_SCROLL_TYPE_RIGHT_EDGE:
      manager_->ScrollToMakeVisible(
          *this, gfx::Rect(r.right(), r.y(), 0, r.height()));
      break;
    case IA2_SCROLL_TYPE_ANYWHERE:
    default:
      manager_->ScrollToMakeVisible(*this, r);
      break;
  }

  static_cast<BrowserAccessibilityManagerWin*>(manager_)
      ->TrackScrollingObject(this);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollToPoint(
    enum IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  if (!instance_active_)
    return E_FAIL;

  gfx::Point scroll_to(x, y);

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    scroll_to -= manager_->GetViewBounds().OffsetFromOrigin();
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    if (parent_)
      scroll_to += parent_->location().OffsetFromOrigin();
  } else {
    return E_INVALIDARG;
  }

  manager_->ScrollToPoint(*this, scroll_to);

  static_cast<BrowserAccessibilityManagerWin*>(manager_)
      ->TrackScrollingObject(this);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_groupPosition(
    LONG* group_level,
    LONG* similar_items_in_group,
    LONG* position_in_group) {
  if (!instance_active_)
    return E_FAIL;

  if (!group_level || !similar_items_in_group || !position_in_group)
    return E_INVALIDARG;

  if (role_ == AccessibilityNodeData::ROLE_LISTBOX_OPTION &&
      parent_ &&
      parent_->role() == AccessibilityNodeData::ROLE_LISTBOX) {
    *group_level = 0;
    *similar_items_in_group = parent_->child_count();
    *position_in_group = index_in_parent_ + 1;
    return S_OK;
  }

  return E_NOTIMPL;
}

//
// IAccessibleImage methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_description(BSTR* desc) {
  if (!instance_active_)
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(
      AccessibilityNodeData::ATTR_DESCRIPTION, desc);
}

STDMETHODIMP BrowserAccessibilityWin::get_imagePosition(
    enum IA2CoordinateType coordinate_type,
    LONG* x,
    LONG* y) {
  if (!instance_active_)
    return E_FAIL;

  if (!x || !y)
    return E_INVALIDARG;

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    HWND parent_hwnd = manager_->GetParentView();
    POINT top_left = {0, 0};
    ::ClientToScreen(parent_hwnd, &top_left);
    *x = location_.x() + top_left.x;
    *y = location_.y() + top_left.y;
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    *x = location_.x();
    *y = location_.y();
    if (parent_) {
      *x -= parent_->location().x();
      *y -= parent_->location().y();
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

  *height = location_.height();
  *width = location_.width();
  return S_OK;
}

//
// IAccessibleTable methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_accessibleAt(
    long row,
    long column,
    IUnknown** accessible) {
  if (!instance_active_)
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  DCHECK_EQ(columns * rows, static_cast<int>(cell_ids_.size()));

  int cell_id = cell_ids_[row * columns + column];
  BrowserAccessibilityWin* cell = GetFromRendererID(cell_id);
  if (cell) {
    *accessible = static_cast<IAccessible*>(cell->NewReference());
    return S_OK;
  }

  *accessible = NULL;
  return E_INVALIDARG;
}

STDMETHODIMP BrowserAccessibilityWin::get_caption(IUnknown** accessible) {
  if (!instance_active_)
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_childIndex(long row,
                                                     long column,
                                                     long* cell_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_index)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  DCHECK_EQ(columns * rows, static_cast<int>(cell_ids_.size()));
  int cell_id = cell_ids_[row * columns + column];
  for (size_t i = 0; i < unique_cell_ids_.size(); ++i) {
    if (unique_cell_ids_[i] == cell_id) {
      *cell_index = (long)i;
      return S_OK;
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnDescription(long column,
                                                            BSTR* description) {
  if (!instance_active_)
    return E_FAIL;

  if (!description)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (column < 0 || column >= columns)
    return E_INVALIDARG;

  for (int i = 0; i < rows; ++i) {
    int cell_id = cell_ids_[i * columns + column];
    BrowserAccessibilityWin* cell = static_cast<BrowserAccessibilityWin*>(
        manager_->GetFromRendererID(cell_id));
    if (cell && cell->role_ == AccessibilityNodeData::ROLE_COLUMN_HEADER) {
      if (cell->name_.size() > 0) {
        *description = SysAllocString(cell->name_.c_str());
        return S_OK;
      }

      return cell->GetStringAttributeAsBstr(
          AccessibilityNodeData::ATTR_DESCRIPTION, description);
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnExtentAt(
    long row,
    long column,
    long* n_columns_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_columns_spanned)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  int cell_id = cell_ids_[row * columns + column];
  BrowserAccessibilityWin* cell = static_cast<BrowserAccessibilityWin*>(
      manager_->GetFromRendererID(cell_id));
  int colspan;
  if (cell &&
      cell->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      colspan >= 1) {
    *n_columns_spanned = colspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnHeader(
    IAccessibleTable** accessible_table,
    long* starting_row_index) {
  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnIndex(long cell_index,
                                                      long* column_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_index)
    return E_INVALIDARG;

  int cell_id_count = static_cast<int>(unique_cell_ids_.size());
  if (cell_index < 0)
    return E_INVALIDARG;
  if (cell_index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids_[cell_index];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
  int col_index;
  if (cell &&
      cell->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_INDEX, &col_index)) {
    *column_index = col_index;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nColumns(long* column_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_count)
    return E_INVALIDARG;

  int columns;
  if (GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns)) {
    *column_count = columns;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nRows(long* row_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_count)
    return E_INVALIDARG;

  int rows;
  if (GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows)) {
    *row_count = rows;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedChildren(long* cell_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_count)
    return E_INVALIDARG;

  // TODO(dmazzoni): add support for selected cells/rows/columns in tables.
  *cell_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedColumns(long* column_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_count)
    return E_INVALIDARG;

  *column_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedRows(long* row_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_count)
    return E_INVALIDARG;

  *row_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowDescription(long row,
                                                         BSTR* description) {
  if (!instance_active_)
    return E_FAIL;

  if (!description)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows)
    return E_INVALIDARG;

  for (int i = 0; i < columns; ++i) {
    int cell_id = cell_ids_[row * columns + i];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
    if (cell && cell->role_ == AccessibilityNodeData::ROLE_ROW_HEADER) {
      if (cell->name_.size() > 0) {
        *description = SysAllocString(cell->name_.c_str());
        return S_OK;
      }

      return cell->GetStringAttributeAsBstr(
          AccessibilityNodeData::ATTR_DESCRIPTION, description);
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowExtentAt(long row,
                                                      long column,
                                                      long* n_rows_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_rows_spanned)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  int cell_id = cell_ids_[row * columns + column];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
  int rowspan;
  if (cell &&
      cell->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      rowspan >= 1) {
    *n_rows_spanned = rowspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowHeader(
    IAccessibleTable** accessible_table,
    long* starting_column_index) {
  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowIndex(long cell_index,
                                                   long* row_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_index)
    return E_INVALIDARG;

  int cell_id_count = static_cast<int>(unique_cell_ids_.size());
  if (cell_index < 0)
    return E_INVALIDARG;
  if (cell_index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids_[cell_index];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
  int cell_row_index;
  if (cell &&
      cell->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_ROW_INDEX, &cell_row_index)) {
    *row_index = cell_row_index;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedChildren(long max_children,
                                                           long** children,
                                                           long* n_children) {
  if (!instance_active_)
    return E_FAIL;

  if (!children || !n_children)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_children = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedColumns(long max_columns,
                                                          long** columns,
                                                          long* n_columns) {
  if (!instance_active_)
    return E_FAIL;

  if (!columns || !n_columns)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_columns = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedRows(long max_rows,
                                                       long** rows,
                                                       long* n_rows) {
  if (!instance_active_)
    return E_FAIL;

  if (!rows || !n_rows)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_rows = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_summary(IUnknown** accessible) {
  if (!instance_active_)
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_isColumnSelected(
    long column,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_isRowSelected(long row,
                                                        boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_isSelected(long row,
                                                     long column,
                                                     boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowColumnExtentsAtIndex(
    long index,
    long* row,
    long* column,
    long* row_extents,
    long* column_extents,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!row || !column || !row_extents || !column_extents || !is_selected)
    return E_INVALIDARG;

  int cell_id_count = static_cast<int>(unique_cell_ids_.size());
  if (index < 0)
    return E_INVALIDARG;
  if (index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids_[index];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
  int rowspan;
  int colspan;
  if (cell &&
      cell->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      cell->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      rowspan >= 1 &&
      colspan >= 1) {
    *row_extents = rowspan;
    *column_extents = colspan;
    return S_OK;
  }

  return S_FALSE;
}

//
// IAccessibleTable2 methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_cellAt(long row,
                                                 long column,
                                                 IUnknown** cell) {
  return get_accessibleAt(row, column, cell);
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedCells(long* cell_count) {
  return get_nSelectedChildren(cell_count);
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedCells(
    IUnknown*** cells,
    long* n_selected_cells) {
  if (!instance_active_)
    return E_FAIL;

  if (!cells || !n_selected_cells)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_selected_cells = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedColumns(long** columns,
                                                          long* n_columns) {
  if (!instance_active_)
    return E_FAIL;

  if (!columns || !n_columns)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_columns = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedRows(long** rows,
                                                       long* n_rows) {
  if (!instance_active_)
    return E_FAIL;

  if (!rows || !n_rows)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_rows = 0;
  return S_OK;
}


//
// IAccessibleTableCell methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_columnExtent(
    long* n_columns_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_columns_spanned)
    return E_INVALIDARG;

  int colspan;
  if (GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      colspan >= 1) {
    *n_columns_spanned = colspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnHeaderCells(
    IUnknown*** cell_accessibles,
    long* n_column_header_cells) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_accessibles || !n_column_header_cells)
    return E_INVALIDARG;

  *n_column_header_cells = 0;

  int column;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_INDEX, &column)) {
    return S_FALSE;
  }

  BrowserAccessibility* table = parent();
  while (table && table->role() != AccessibilityNodeData::ROLE_TABLE)
    table = table->parent();
  if (!table) {
    NOTREACHED();
    return S_FALSE;
  }

  int columns;
  int rows;
  if (!table->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !table->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows)) {
    return S_FALSE;
  }
  if (columns <= 0 || rows <= 0 || column < 0 || column >= columns)
    return S_FALSE;

  for (int i = 0; i < rows; ++i) {
    int cell_id = table->cell_ids()[i * columns + column];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
    if (cell && cell->role_ == AccessibilityNodeData::ROLE_COLUMN_HEADER)
      (*n_column_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(CoTaskMemAlloc(
      (*n_column_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int i = 0; i < rows; ++i) {
    int cell_id = table->cell_ids()[i * columns + column];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
    if (cell && cell->role_ == AccessibilityNodeData::ROLE_COLUMN_HEADER) {
      (*cell_accessibles)[index] =
          static_cast<IAccessible*>(cell->NewReference());
      ++index;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnIndex(long* column_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_index)
    return E_INVALIDARG;

  int column;
  if (GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_INDEX, &column)) {
    *column_index = column;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowExtent(long* n_rows_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_rows_spanned)
    return E_INVALIDARG;

  int rowspan;
  if (GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      rowspan >= 1) {
    *n_rows_spanned = rowspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowHeaderCells(
    IUnknown*** cell_accessibles,
    long* n_row_header_cells) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_accessibles || !n_row_header_cells)
    return E_INVALIDARG;

  *n_row_header_cells = 0;

  int row;
  if (!GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_ROW_INDEX, &row)) {
    return S_FALSE;
  }

  BrowserAccessibility* table = parent();
  while (table && table->role() != AccessibilityNodeData::ROLE_TABLE)
    table = table->parent();
  if (!table) {
    NOTREACHED();
    return S_FALSE;
  }

  int columns;
  int rows;
  if (!table->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !table->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_ROW_COUNT, &rows)) {
    return S_FALSE;
  }
  if (columns <= 0 || rows <= 0 || row < 0 || row >= rows)
    return S_FALSE;

  for (int i = 0; i < columns; ++i) {
    int cell_id = table->cell_ids()[row * columns + i];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
    if (cell && cell->role_ == AccessibilityNodeData::ROLE_ROW_HEADER)
      (*n_row_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(CoTaskMemAlloc(
      (*n_row_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int i = 0; i < columns; ++i) {
    int cell_id = table->cell_ids()[row * columns + i];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->ToBrowserAccessibilityWin();
    if (cell && cell->role_ == AccessibilityNodeData::ROLE_ROW_HEADER) {
      (*cell_accessibles)[index] =
          static_cast<IAccessible*>(cell->NewReference());
      ++index;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowIndex(long* row_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_index)
    return E_INVALIDARG;

  int row;
  if (GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_CELL_ROW_INDEX, &row)) {
    *row_index = row;
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_isSelected(boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowColumnExtents(
    long* row_index,
    long* column_index,
    long* row_extents,
    long* column_extents,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_index ||
      !column_index ||
      !row_extents ||
      !column_extents ||
      !is_selected) {
    return E_INVALIDARG;
  }

  int row;
  int column;
  int rowspan;
  int colspan;
  if (GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_CELL_ROW_INDEX, &row) &&
      GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_INDEX, &column) &&
      GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan)) {
    *row_index = row;
    *column_index = column;
    *row_extents = rowspan;
    *column_extents = colspan;
    *is_selected = false;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_table(IUnknown** table) {
  if (!instance_active_)
    return E_FAIL;

  if (!table)
    return E_INVALIDARG;


  int row;
  int column;
  GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_CELL_ROW_INDEX, &row);
  GetIntAttribute(AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_INDEX, &column);

  BrowserAccessibility* find_table = parent();
  while (find_table && find_table->role() != AccessibilityNodeData::ROLE_TABLE)
    find_table = find_table->parent();
  if (!find_table) {
    NOTREACHED();
    return S_FALSE;
  }

  *table = static_cast<IAccessibleTable*>(
      find_table->ToBrowserAccessibilityWin()->NewReference());

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

  *n_characters = TextForIAccessibleText().length();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_caretOffset(LONG* offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  *offset = 0;
  if (role_ == AccessibilityNodeData::ROLE_TEXT_FIELD ||
      role_ == AccessibilityNodeData::ROLE_TEXTAREA) {
    int sel_start = 0;
    if (GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_START,
                        &sel_start))
      *offset = sel_start;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelections(LONG* n_selections) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_selections)
    return E_INVALIDARG;

  *n_selections = 0;
  if (role_ == AccessibilityNodeData::ROLE_TEXT_FIELD ||
      role_ == AccessibilityNodeData::ROLE_TEXTAREA) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_START,
                        &sel_start) &&
        GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_END, &sel_end) &&
        sel_start != sel_end)
      *n_selections = 1;
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

  *start_offset = 0;
  *end_offset = 0;
  if (role_ == AccessibilityNodeData::ROLE_TEXT_FIELD ||
      role_ == AccessibilityNodeData::ROLE_TEXTAREA) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetIntAttribute(
            AccessibilityNodeData::ATTR_TEXT_SEL_START, &sel_start) &&
        GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_END, &sel_end)) {
      *start_offset = sel_start;
      *end_offset = sel_end;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_text(LONG start_offset,
                                               LONG end_offset,
                                               BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  const string16& text_str = TextForIAccessibleText();

  // Handle special text offsets.
  HandleSpecialTextOffset(text_str, &start_offset);
  HandleSpecialTextOffset(text_str, &end_offset);

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
    LONG* start_offset,
    LONG* end_offset,
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

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textBeforeOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
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

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textAfterOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
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
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_newText(IA2TextSegment* new_text) {
  if (!instance_active_)
    return E_FAIL;

  if (!new_text)
    return E_INVALIDARG;

  string16 text = TextForIAccessibleText();

  new_text->text = SysAllocString(text.c_str());
  new_text->start = 0;
  new_text->end = static_cast<long>(text.size());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_oldText(IA2TextSegment* old_text) {
  if (!instance_active_)
    return E_FAIL;

  if (!old_text)
    return E_INVALIDARG;

  old_text->text = SysAllocString(old_text_.c_str());
  old_text->start = 0;
  old_text->end = static_cast<long>(old_text_.size());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_offsetAtPoint(
    LONG x,
    LONG y,
    enum IA2CoordinateType coord_type,
    LONG* offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement this. We're returning S_OK for now so that
  // screen readers still return partially accurate results rather than
  // completely failing.
  *offset = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollSubstringTo(
    LONG start_index,
    LONG end_index,
    enum IA2ScrollType scroll_type) {
  // TODO(dmazzoni): adjust this for the start and end index, too.
  return scrollTo(scroll_type);
}

STDMETHODIMP BrowserAccessibilityWin::scrollSubstringToPoint(
    LONG start_index,
    LONG end_index,
    enum IA2CoordinateType coordinate_type,
    LONG x, LONG y) {
  // TODO(dmazzoni): adjust this for the start and end index, too.
  return scrollToPoint(coordinate_type, x, y);
}

STDMETHODIMP BrowserAccessibilityWin::addSelection(LONG start_offset,
                                                   LONG end_offset) {
  if (!instance_active_)
    return E_FAIL;

  const string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &start_offset);
  HandleSpecialTextOffset(text_str, &end_offset);

  manager_->SetTextSelection(*this, start_offset, end_offset);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::removeSelection(LONG selection_index) {
  if (!instance_active_)
    return E_FAIL;

  if (selection_index != 0)
    return E_INVALIDARG;

  manager_->SetTextSelection(*this, 0, 0);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::setCaretOffset(LONG offset) {
  if (!instance_active_)
    return E_FAIL;

  const string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &offset);
  manager_->SetTextSelection(*this, offset, offset);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::setSelection(LONG selection_index,
                                                   LONG start_offset,
                                                   LONG end_offset) {
  if (!instance_active_)
    return E_FAIL;

  if (selection_index != 0)
    return E_INVALIDARG;

  const string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &start_offset);
  HandleSpecialTextOffset(text_str, &end_offset);

  manager_->SetTextSelection(*this, start_offset, end_offset);
  return S_OK;
}

//
// IAccessibleHypertext methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nHyperlinks(long* hyperlink_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!hyperlink_count)
    return E_INVALIDARG;

  *hyperlink_count = hyperlink_offset_to_index_.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_hyperlink(
    long index,
    IAccessibleHyperlink** hyperlink) {
  if (!instance_active_)
    return E_FAIL;

  if (!hyperlink ||
      index < 0 ||
      index >= static_cast<long>(hyperlinks_.size())) {
    return E_INVALIDARG;
  }

  BrowserAccessibilityWin* child =
      children_[hyperlinks_[index]]->ToBrowserAccessibilityWin();
  *hyperlink = static_cast<IAccessibleHyperlink*>(child->NewReference());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_hyperlinkIndex(
    long char_index,
    long* hyperlink_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!hyperlink_index)
    return E_INVALIDARG;

  *hyperlink_index = -1;

  if (char_index < 0 || char_index >= static_cast<long>(hypertext_.size()))
    return E_INVALIDARG;

  std::map<int32, int32>::iterator it =
      hyperlink_offset_to_index_.find(char_index);
  if (it == hyperlink_offset_to_index_.end())
    return E_FAIL;

  *hyperlink_index = it->second;
  return S_OK;
}

//
// IAccessibleValue methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_currentValue(VARIANT* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(
          AccessibilityNodeData::ATTR_VALUE_FOR_RANGE, &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_minimumValue(VARIANT* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(AccessibilityNodeData::ATTR_MIN_VALUE_FOR_RANGE,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_maximumValue(VARIANT* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(AccessibilityNodeData::ATTR_MAX_VALUE_FOR_RANGE,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::setCurrentValue(VARIANT new_value) {
  // TODO(dmazzoni): Implement this.
  return E_NOTIMPL;
}

//
// ISimpleDOMDocument methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_URL(BSTR* url) {
  if (!instance_active_)
    return E_FAIL;

  if (!url)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(AccessibilityNodeData::ATTR_DOC_URL, url);
}

STDMETHODIMP BrowserAccessibilityWin::get_title(BSTR* title) {
  if (!instance_active_)
    return E_FAIL;

  if (!title)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(AccessibilityNodeData::ATTR_DOC_TITLE, title);
}

STDMETHODIMP BrowserAccessibilityWin::get_mimeType(BSTR* mime_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!mime_type)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(
      AccessibilityNodeData::ATTR_DOC_MIMETYPE, mime_type);
}

STDMETHODIMP BrowserAccessibilityWin::get_docType(BSTR* doc_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!doc_type)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(
      AccessibilityNodeData::ATTR_DOC_DOCTYPE, doc_type);
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
  if (GetStringAttribute(AccessibilityNodeData::ATTR_HTML_TAG, &tag))
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
    BSTR* style_properties,
    BSTR* style_values,
    unsigned short *num_style_properties)  {
  if (!instance_active_)
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  string16 display;
  if (max_style_properties == 0 ||
      !GetStringAttribute(AccessibilityNodeData::ATTR_DISPLAY, &display)) {
    *num_style_properties = 0;
    return S_OK;
  }

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

  for (unsigned short i = 0; i < num_style_properties; ++i) {
    string16 name = (LPCWSTR)style_properties[i];
    StringToLowerASCII(&name);
    if (name == L"display") {
      string16 display;
      GetStringAttribute(AccessibilityNodeData::ATTR_DISPLAY, &display);
      style_values[i] = SysAllocString(display.c_str());
    } else {
      style_values[i] = NULL;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollTo(boolean placeTopLeft) {
  return scrollTo(placeTopLeft ?
      IA2_SCROLL_TYPE_TOP_LEFT : IA2_SCROLL_TYPE_ANYWHERE);
}

STDMETHODIMP BrowserAccessibilityWin::get_parentNode(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  *node = parent_->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_firstChild(ISimpleDOMNode** node)  {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.empty()) {
    *node = NULL;
    return S_FALSE;
  }

  *node = children_[0]->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_lastChild(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.empty()) {
    *node = NULL;
    return S_FALSE;
  }

  *node = (*children_.rbegin())->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_previousSibling(
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (!parent_ || index_in_parent_ <= 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = parent_->children()[index_in_parent_ - 1]->
      ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nextSibling(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (!parent_ ||
      index_in_parent_ < 0 ||
      index_in_parent_ >= static_cast<int>(parent_->children().size()) - 1) {
    *node = NULL;
    return S_FALSE;
  }

  *node = parent_->children()[index_in_parent_ + 1]->
      ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_childAt(
    unsigned int child_index,
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (child_index < children_.size()) {
    *node = NULL;
    return S_FALSE;
  }

  *node = children_[child_index]->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
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

STDMETHODIMP BrowserAccessibilityWin::QueryService(REFGUID guidService,
                                                   REFIID riid,
                                                   void** object) {
  if (!instance_active_)
    return E_FAIL;

  if (guidService == GUID_IAccessibleContentDocument) {
    // Special Mozilla extension: return the accessible for the root document.
    // Screen readers use this to distinguish between a document loaded event
    // on the root document vs on an iframe.
    return manager_->GetRoot()->ToBrowserAccessibilityWin()->QueryInterface(
        IID_IAccessible2, object);
  }

  if (guidService == IID_IAccessible ||
      guidService == IID_IAccessible2 ||
      guidService == IID_IAccessibleAction ||
      guidService == IID_IAccessibleHyperlink ||
      guidService == IID_IAccessibleHypertext ||
      guidService == IID_IAccessibleImage ||
      guidService == IID_IAccessibleTable ||
      guidService == IID_IAccessibleTable2 ||
      guidService == IID_IAccessibleTableCell ||
      guidService == IID_IAccessibleText ||
      guidService == IID_IAccessibleValue ||
      guidService == IID_ISimpleDOMDocument ||
      guidService == IID_ISimpleDOMNode ||
      guidService == IID_ISimpleDOMText ||
      guidService == GUID_ISimpleDOM) {
    return QueryInterface(riid, object);
  }

  // We only support the IAccessibleEx interface on Windows 8 and above. This
  // is needed for the on-screen Keyboard to show up in metro mode, when the
  // user taps an editable portion on the page.
  // All methods in the IAccessibleEx interface are unimplemented.
  if (riid == IID_IAccessibleEx &&
      base::win::GetVersion() >= base::win::VERSION_WIN8) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

STDMETHODIMP BrowserAccessibilityWin::GetPatternProvider(PATTERNID id,
                                                         IUnknown** provider) {
  DVLOG(1) << "In Function: "
           << __FUNCTION__
           << " for pattern id: "
           << id;
  if (id == UIA_ValuePatternId || id == UIA_TextPatternId) {
    if (IsEditableText()) {
      // The BrowserAccessibilityManager keeps track of instances when
      // we don't want to show the on-screen keyboard.
      if (!manager_->IsOSKAllowed(GetGlobalBoundsRect()))
        return E_NOTIMPL;

      DVLOG(1) << "Returning UIA text provider";
      base::win::UIATextProvider::CreateTextProvider(true, provider);
      return S_OK;
    }
  }
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::GetPropertyValue(PROPERTYID id,
                                                       VARIANT* ret) {
  DVLOG(1) << "In Function: "
           << __FUNCTION__
           << " for property id: "
           << id;
  V_VT(ret) = VT_EMPTY;
  if (id == UIA_ControlTypePropertyId) {
    if (IsEditableText()) {
      V_VT(ret) = VT_I4;
      ret->lVal = UIA_EditControlTypeId;
      DVLOG(1) << "Returning Edit control type";
    } else {
      DVLOG(1) << "Returning empty control type";
    }
  }
  return S_OK;
}

//
// CComObjectRootEx methods.
//

HRESULT WINAPI BrowserAccessibilityWin::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID iid,
    void** object) {
  if (iid == IID_IAccessibleImage) {
    if (ia_role_ != ROLE_SYSTEM_GRAPHIC) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTable || iid == IID_IAccessibleTable2) {
    if (ia_role_ != ROLE_SYSTEM_TABLE) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTableCell) {
    if (ia_role_ != ROLE_SYSTEM_CELL) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleValue) {
    if (ia_role_ != ROLE_SYSTEM_PROGRESSBAR &&
        ia_role_ != ROLE_SYSTEM_SCROLLBAR &&
        ia_role_ != ROLE_SYSTEM_SLIDER) {
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
void BrowserAccessibilityWin::PreInitialize() {
  BrowserAccessibility::PreInitialize();

  InitRoleAndState();

  // Expose the "display" and "tag" attributes.
  StringAttributeToIA2(AccessibilityNodeData::ATTR_DISPLAY, "display");
  StringAttributeToIA2(AccessibilityNodeData::ATTR_HTML_TAG, "tag");
  StringAttributeToIA2(AccessibilityNodeData::ATTR_ROLE, "xml-roles");

  // Expose "level" attribute for headings, trees, etc.
  IntAttributeToIA2(AccessibilityNodeData::ATTR_HIERARCHICAL_LEVEL, "level");

  // Expose the set size and position in set for listbox options.
  if (role_ == AccessibilityNodeData::ROLE_LISTBOX_OPTION &&
      parent_ &&
      parent_->role() == AccessibilityNodeData::ROLE_LISTBOX) {
    ia2_attributes_.push_back(
        L"setsize:" + base::IntToString16(parent_->child_count()));
    ia2_attributes_.push_back(
        L"setsize:" + base::IntToString16(index_in_parent_ + 1));
  }

  if (ia_role_ == ROLE_SYSTEM_CHECKBUTTON ||
      ia_role_ == ROLE_SYSTEM_RADIOBUTTON ||
      ia2_role_ == IA2_ROLE_TOGGLE_BUTTON) {
    ia2_attributes_.push_back(L"checkable:true");
  }

  // Expose live region attributes.
  StringAttributeToIA2(AccessibilityNodeData::ATTR_LIVE_STATUS, "live");
  StringAttributeToIA2(AccessibilityNodeData::ATTR_LIVE_RELEVANT, "relevant");
  BoolAttributeToIA2(AccessibilityNodeData::ATTR_LIVE_ATOMIC, "atomic");
  BoolAttributeToIA2(AccessibilityNodeData::ATTR_LIVE_BUSY, "busy");

  // Expose container live region attributes.
  StringAttributeToIA2(AccessibilityNodeData::ATTR_CONTAINER_LIVE_STATUS,
                       "container-live");
  StringAttributeToIA2(AccessibilityNodeData::ATTR_CONTAINER_LIVE_RELEVANT,
                       "container-relevant");
  BoolAttributeToIA2(AccessibilityNodeData::ATTR_CONTAINER_LIVE_ATOMIC,
                     "container-atomic");
  BoolAttributeToIA2(AccessibilityNodeData::ATTR_CONTAINER_LIVE_BUSY,
                     "container-busy");

  // Expose slider value.
  if (ia_role_ == ROLE_SYSTEM_PROGRESSBAR ||
      ia_role_ == ROLE_SYSTEM_SCROLLBAR ||
      ia_role_ == ROLE_SYSTEM_SLIDER) {
    float fval;
    if (value_.empty() &&
        GetFloatAttribute(AccessibilityNodeData::ATTR_VALUE_FOR_RANGE, &fval)) {
      // TODO(dmazzoni): Use ICU to localize this?
      value_ = UTF8ToUTF16(base::DoubleToString(fval));
    }
    ia2_attributes_.push_back(L"valuetext:" + value_);
  }

  // Expose color well value.
  if (ia2_role_ == IA2_ROLE_COLOR_CHOOSER) {
    int r, g, b;
    GetIntAttribute(AccessibilityNodeData::ATTR_COLOR_VALUE_RED, &r);
    GetIntAttribute(AccessibilityNodeData::ATTR_COLOR_VALUE_GREEN, &g);
    GetIntAttribute(AccessibilityNodeData::ATTR_COLOR_VALUE_BLUE, &b);
    value_ = base::IntToString16((r * 100) / 255) + L"% red " +
             base::IntToString16((g * 100) / 255) + L"% green " +
             base::IntToString16((b * 100) / 255) + L"% blue";
  }

  // Expose table cell index.
  if (ia_role_ == ROLE_SYSTEM_CELL) {
    BrowserAccessibility* table = parent();
    while (table && table->role() != AccessibilityNodeData::ROLE_TABLE)
      table = table->parent();
    if (table) {
      const std::vector<int32>& unique_cell_ids = table->unique_cell_ids();
      for (size_t i = 0; i < unique_cell_ids.size(); ++i) {
        if (unique_cell_ids[i] == renderer_id_) {
          ia2_attributes_.push_back(
              string16(L"table-cell-index:") + base::IntToString16(i));
        }
      }
    }
  }

  // The calculation of the accessible name of an element has been
  // standardized in the HTML to Platform Accessibility APIs Implementation
  // Guide (http://www.w3.org/TR/html-aapi/). In order to return the
  // appropriate accessible name on Windows, we need to apply some logic
  // to the fields we get from WebKit.
  //
  // TODO(dmazzoni): move most of this logic into WebKit.
  //
  // WebKit gives us:
  //
  //   name: the default name, e.g. inner text
  //   title ui element: a reference to a <label> element on the same
  //       page that labels this node.
  //   description: accessible labels that override the default name:
  //       aria-label or aria-labelledby or aria-describedby
  //   help: the value of the "title" attribute
  //
  // On Windows, the logic we apply lets some fields take precedence and
  // always returns the primary name in "name" and the secondary name,
  // if any, in "description".

  string16 description, help, title_attr;
  int title_elem_id = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_TITLE_UI_ELEMENT, &title_elem_id);
  GetStringAttribute(AccessibilityNodeData::ATTR_DESCRIPTION, &description);
  GetStringAttribute(AccessibilityNodeData::ATTR_HELP, &help);

  // WebKit annoyingly puts the title in the description if there's no other
  // description, which just confuses the rest of the logic. Put it back.
  // Now "help" is always the value of the "title" attribute, if present.
  if (GetHtmlAttribute("title", &title_attr) &&
      description == title_attr &&
      help.empty()) {
    help = description;
    description.clear();
    string_attributes_[AccessibilityNodeData::ATTR_DESCRIPTION].clear();
    string_attributes_[AccessibilityNodeData::ATTR_HELP] = help;
  }

  // Now implement the main logic: the descripion should become the name if
  // it's nonempty, and the help should become the description if
  // there's no description - or the name if there's no name or description.
  if (!description.empty()) {
    name_ = description;
    description.clear();
    string_attributes_[AccessibilityNodeData::ATTR_DESCRIPTION] = description;
  }
  if (!help.empty() && description.empty()) {
    description = help;
    string_attributes_[AccessibilityNodeData::ATTR_DESCRIPTION] = help;
    string_attributes_[AccessibilityNodeData::ATTR_HELP].clear();
  }
  if (!description.empty() && name_.empty() && !title_elem_id) {
    name_ = description;
    description.clear();
    string_attributes_[AccessibilityNodeData::ATTR_DESCRIPTION].clear();
  }

  // If it's a text field, also consider the placeholder.
  string16 placeholder;
  if (role_ == AccessibilityNodeData::ROLE_TEXT_FIELD &&
      HasState(AccessibilityNodeData::STATE_FOCUSABLE) &&
      GetHtmlAttribute("placeholder", &placeholder)) {
    if (name_.empty() && !title_elem_id) {
      name_ = placeholder;
    } else if (description.empty()) {
      description = placeholder;
      string_attributes_[AccessibilityNodeData::ATTR_DESCRIPTION] = description;
    }
  }

  // For certain roles (listbox option, static text, and list marker)
  // WebKit stores the main accessible text in the "value" - swap it so
  // that it's the "name".
  if (name_.empty() &&
      (role_ == AccessibilityNodeData::ROLE_LISTBOX_OPTION ||
       role_ == AccessibilityNodeData::ROLE_STATIC_TEXT ||
       role_ == AccessibilityNodeData::ROLE_LIST_MARKER)) {
    name_.swap(value_);
  }

  // If this doesn't have a value and is linked then set its value to the url
  // attribute. This allows screen readers to read an empty link's destination.
  string16 url;
  if (value_.empty() && (ia_state_ & STATE_SYSTEM_LINKED))
    GetStringAttribute(AccessibilityNodeData::ATTR_URL, &value_);

  // Clear any old relationships between this node and other nodes.
  for (size_t i = 0; i < relations_.size(); ++i)
    relations_[i]->Release();
  relations_.clear();

  // Handle title UI element.
  if (title_elem_id) {
    // Add a labelled by relationship.
    CComObject<BrowserAccessibilityRelation>* relation;
    HRESULT hr = CComObject<BrowserAccessibilityRelation>::CreateInstance(
        &relation);
    DCHECK(SUCCEEDED(hr));
    relation->AddRef();
    relation->Initialize(this, IA2_RELATION_LABELLED_BY);
    relation->AddTarget(title_elem_id);
    relations_.push_back(relation);
  }
}

void BrowserAccessibilityWin::PostInitialize() {
  BrowserAccessibility::PostInitialize();

  // Construct the hypertext for this node.
  hyperlink_offset_to_index_.clear();
  hyperlinks_.clear();
  hypertext_.clear();
  for (unsigned int i = 0; i < children().size(); ++i) {
    BrowserAccessibility* child = children()[i];
    if (child->role() == AccessibilityNodeData::ROLE_STATIC_TEXT) {
      hypertext_ += child->name();
    } else {
      hyperlink_offset_to_index_[hypertext_.size()] = hyperlinks_.size();
      hypertext_ += kEmbeddedCharacter;
      hyperlinks_.push_back(i);
    }
  }
  DCHECK_EQ(hyperlink_offset_to_index_.size(), hyperlinks_.size());

  // Fire an event when an alert first appears.
  if (role_ == AccessibilityNodeData::ROLE_ALERT && first_time_)
    manager_->NotifyAccessibilityEvent(AccessibilityNotificationAlert, this);

  // Fire events if text has changed.
  string16 text = TextForIAccessibleText();
  if (previous_text_ != text) {
    if (!previous_text_.empty() && !text.empty()) {
      manager_->NotifyAccessibilityEvent(
          AccessibilityNotificationObjectShow, this);
    }

    // TODO(dmazzoni): Look into HIDE events, too.

    old_text_ = previous_text_;
    previous_text_ = text;
  }

  // Fire events if the state has changed.
  if (!first_time_ && ia_state_ != old_ia_state_) {
    // Normally focus events are handled elsewhere, however
    // focus for managed descendants is platform-specific.
    // Fire a focus event if the focused descendant in a multi-select
    // list box changes.
    if (role_ == AccessibilityNodeData::ROLE_LISTBOX_OPTION &&
        (ia_state_ & STATE_SYSTEM_FOCUSABLE) &&
        (ia_state_ & STATE_SYSTEM_SELECTABLE) &&
        (ia_state_ & STATE_SYSTEM_FOCUSED) &&
        !(old_ia_state_ & STATE_SYSTEM_FOCUSED)) {
      ::NotifyWinEvent(EVENT_OBJECT_FOCUS, manager_->GetParentView(),
                       OBJID_CLIENT, child_id());
    }

    if ((ia_state_ & STATE_SYSTEM_SELECTED) &&
        !(old_ia_state_ & STATE_SYSTEM_SELECTED)) {
      ::NotifyWinEvent(EVENT_OBJECT_SELECTIONADD, manager_->GetParentView(),
                       OBJID_CLIENT, child_id());
    } else if (!(ia_state_ & STATE_SYSTEM_SELECTED) &&
               (old_ia_state_ & STATE_SYSTEM_SELECTED)) {
      ::NotifyWinEvent(EVENT_OBJECT_SELECTIONREMOVE, manager_->GetParentView(),
                       OBJID_CLIENT, child_id());
    }

    old_ia_state_ = ia_state_;
  }

  first_time_ = false;
}

void BrowserAccessibilityWin::NativeAddReference() {
  AddRef();
}

void BrowserAccessibilityWin::NativeReleaseReference() {
  Release();
}

bool BrowserAccessibilityWin::IsNative() const {
  return true;
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
    return children_[child_id - 1]->ToBrowserAccessibilityWin();

  return manager_->GetFromChildID(child_id)->ToBrowserAccessibilityWin();
}

HRESULT BrowserAccessibilityWin::GetStringAttributeAsBstr(
    AccessibilityNodeData::StringAttribute attribute,
    BSTR* value_bstr) {
  string16 str;

  if (!GetStringAttribute(attribute, &str))
    return S_FALSE;

  if (str.empty())
    return S_FALSE;

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);

  return S_OK;
}

void BrowserAccessibilityWin::StringAttributeToIA2(
    AccessibilityNodeData::StringAttribute attribute,
    const char* ia2_attr) {
  string16 value;
  if (GetStringAttribute(attribute, &value))
    ia2_attributes_.push_back(ASCIIToUTF16(ia2_attr) + L":" + value);
}

void BrowserAccessibilityWin::BoolAttributeToIA2(
    AccessibilityNodeData::BoolAttribute attribute,
    const char* ia2_attr) {
  bool value;
  if (GetBoolAttribute(attribute, &value)) {
    ia2_attributes_.push_back((ASCIIToUTF16(ia2_attr) + L":") +
                              (value ? L"true" : L"false"));
  }
}

void BrowserAccessibilityWin::IntAttributeToIA2(
    AccessibilityNodeData::IntAttribute attribute,
    const char* ia2_attr) {
  int value;
  if (GetIntAttribute(attribute, &value))
    ia2_attributes_.push_back(ASCIIToUTF16(ia2_attr) + L":" +
                              base::IntToString16(value));
}

const string16& BrowserAccessibilityWin::TextForIAccessibleText() {
  if (IsEditableText())
    return value_;
  return (role_ == AccessibilityNodeData::ROLE_STATIC_TEXT) ?
      name_ : hypertext_;
}

void BrowserAccessibilityWin::HandleSpecialTextOffset(const string16& text,
                                                      LONG* offset) {
  if (*offset == IA2_TEXT_OFFSET_LENGTH)
    *offset = static_cast<LONG>(text.size());
  else if (*offset == IA2_TEXT_OFFSET_CARET)
    get_caretOffset(offset);
}

ui::TextBoundaryType BrowserAccessibilityWin::IA2TextBoundaryToTextBoundary(
    IA2TextBoundaryType ia2_boundary) {
  switch(ia2_boundary) {
    case IA2_TEXT_BOUNDARY_CHAR: return ui::CHAR_BOUNDARY;
    case IA2_TEXT_BOUNDARY_WORD: return ui::WORD_BOUNDARY;
    case IA2_TEXT_BOUNDARY_LINE: return ui::LINE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_SENTENCE: return ui::SENTENCE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_PARAGRAPH: return ui::PARAGRAPH_BOUNDARY;
    case IA2_TEXT_BOUNDARY_ALL: return ui::ALL_BOUNDARY;
    default:
      NOTREACHED();
      return ui::CHAR_BOUNDARY;
  }
}

LONG BrowserAccessibilityWin::FindBoundary(
    const string16& text,
    IA2TextBoundaryType ia2_boundary,
    LONG start_offset,
    ui::TextBoundaryDirection direction) {
  HandleSpecialTextOffset(text, &start_offset);
  ui::TextBoundaryType boundary = IA2TextBoundaryToTextBoundary(ia2_boundary);
  return ui::FindAccessibleTextBoundary(
      text, line_breaks_, boundary, start_offset, direction);
}

BrowserAccessibilityWin* BrowserAccessibilityWin::GetFromRendererID(
    int32 renderer_id) {
  return manager_->GetFromRendererID(renderer_id)->ToBrowserAccessibilityWin();
}

void BrowserAccessibilityWin::InitRoleAndState() {
  ia_state_ = 0;
  ia2_state_ = IA2_STATE_OPAQUE;
  ia2_attributes_.clear();

  if (HasState(AccessibilityNodeData::STATE_BUSY))
    ia_state_ |= STATE_SYSTEM_BUSY;
  if (HasState(AccessibilityNodeData::STATE_CHECKED))
    ia_state_ |= STATE_SYSTEM_CHECKED;
  if (HasState(AccessibilityNodeData::STATE_COLLAPSED))
    ia_state_ |= STATE_SYSTEM_COLLAPSED;
  if (HasState(AccessibilityNodeData::STATE_EXPANDED))
    ia_state_ |= STATE_SYSTEM_EXPANDED;
  if (HasState(AccessibilityNodeData::STATE_FOCUSABLE))
    ia_state_ |= STATE_SYSTEM_FOCUSABLE;
  if (HasState(AccessibilityNodeData::STATE_HASPOPUP))
    ia_state_ |= STATE_SYSTEM_HASPOPUP;
  if (HasState(AccessibilityNodeData::STATE_HOTTRACKED))
    ia_state_ |= STATE_SYSTEM_HOTTRACKED;
  if (HasState(AccessibilityNodeData::STATE_INDETERMINATE))
    ia_state_ |= STATE_SYSTEM_INDETERMINATE;
  if (HasState(AccessibilityNodeData::STATE_INVISIBLE))
    ia_state_ |= STATE_SYSTEM_INVISIBLE;
  if (HasState(AccessibilityNodeData::STATE_LINKED))
    ia_state_ |= STATE_SYSTEM_LINKED;
  if (HasState(AccessibilityNodeData::STATE_MULTISELECTABLE)) {
    ia_state_ |= STATE_SYSTEM_EXTSELECTABLE;
    ia_state_ |= STATE_SYSTEM_MULTISELECTABLE;
  }
  // TODO(ctguil): Support STATE_SYSTEM_EXTSELECTABLE/accSelect.
  if (HasState(AccessibilityNodeData::STATE_OFFSCREEN))
    ia_state_ |= STATE_SYSTEM_OFFSCREEN;
  if (HasState(AccessibilityNodeData::STATE_PRESSED))
    ia_state_ |= STATE_SYSTEM_PRESSED;
  if (HasState(AccessibilityNodeData::STATE_PROTECTED))
    ia_state_ |= STATE_SYSTEM_PROTECTED;
  if (HasState(AccessibilityNodeData::STATE_REQUIRED))
    ia2_state_ |= IA2_STATE_REQUIRED;
  if (HasState(AccessibilityNodeData::STATE_SELECTABLE))
    ia_state_ |= STATE_SYSTEM_SELECTABLE;
  if (HasState(AccessibilityNodeData::STATE_SELECTED))
    ia_state_ |= STATE_SYSTEM_SELECTED;
  if (HasState(AccessibilityNodeData::STATE_TRAVERSED))
    ia_state_ |= STATE_SYSTEM_TRAVERSED;
  if (HasState(AccessibilityNodeData::STATE_UNAVAILABLE))
    ia_state_ |= STATE_SYSTEM_UNAVAILABLE;
  if (HasState(AccessibilityNodeData::STATE_VERTICAL)) {
    ia2_state_ |= IA2_STATE_VERTICAL;
  } else {
    ia2_state_ |= IA2_STATE_HORIZONTAL;
  }
  if (HasState(AccessibilityNodeData::STATE_VISITED))
    ia_state_ |= STATE_SYSTEM_TRAVERSED;

  // WebKit marks everything as readonly unless it's editable text, so if it's
  // not readonly, mark it as editable now. The final computation of the
  // READONLY state for MSAA is below, after the switch.
  if (!HasState(AccessibilityNodeData::STATE_READONLY))
    ia2_state_ |= IA2_STATE_EDITABLE;

  string16 invalid;
  if (GetHtmlAttribute("aria-invalid", &invalid))
    ia2_state_ |= IA2_STATE_INVALID_ENTRY;

  bool mixed = false;
  GetBoolAttribute(AccessibilityNodeData::ATTR_BUTTON_MIXED, &mixed);
  if (mixed)
    ia_state_ |= STATE_SYSTEM_MIXED;

  bool editable = false;
  GetBoolAttribute(AccessibilityNodeData::ATTR_CAN_SET_VALUE, &editable);
  if (editable)
    ia2_state_ |= IA2_STATE_EDITABLE;

  string16 html_tag;
  GetStringAttribute(AccessibilityNodeData::ATTR_HTML_TAG, &html_tag);
  ia_role_ = 0;
  ia2_role_ = 0;
  switch (role_) {
    case AccessibilityNodeData::ROLE_ALERT:
      ia_role_ = ROLE_SYSTEM_ALERT;
      break;
    case AccessibilityNodeData::ROLE_ALERT_DIALOG:
      ia_role_ = ROLE_SYSTEM_DIALOG;
      break;
    case AccessibilityNodeData::ROLE_APPLICATION:
      ia_role_ = ROLE_SYSTEM_APPLICATION;
      break;
    case AccessibilityNodeData::ROLE_ARTICLE:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_BUSY_INDICATOR:
      ia_role_ = ROLE_SYSTEM_ANIMATION;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_BUTTON:
      ia_role_ = ROLE_SYSTEM_PUSHBUTTON;
      bool is_aria_pressed_defined;
      bool is_mixed;
      if (GetAriaTristate("aria-pressed", &is_aria_pressed_defined, &is_mixed))
        ia_state_ |= STATE_SYSTEM_PRESSED;
      if (is_aria_pressed_defined)
        ia2_role_ = IA2_ROLE_TOGGLE_BUTTON;
      if (is_mixed)
        ia_state_ |= STATE_SYSTEM_MIXED;
      break;
    case AccessibilityNodeData::ROLE_CANVAS:
      ia_role_ = ROLE_SYSTEM_GRAPHIC;
      break;
    case AccessibilityNodeData::ROLE_CANVAS_WITH_FALLBACK_CONTENT:
      role_name_ = L"canvas";
      ia2_role_ = IA2_ROLE_CANVAS;
      break;
    case AccessibilityNodeData::ROLE_CELL:
      ia_role_ = ROLE_SYSTEM_CELL;
      break;
    case AccessibilityNodeData::ROLE_CHECKBOX:
      ia_role_ = ROLE_SYSTEM_CHECKBUTTON;
      break;
    case AccessibilityNodeData::ROLE_COLOR_WELL:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_COLOR_CHOOSER;
      break;
    case AccessibilityNodeData::ROLE_COLUMN:
      ia_role_ = ROLE_SYSTEM_COLUMN;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_COLUMN_HEADER:
      ia_role_ = ROLE_SYSTEM_COLUMNHEADER;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_COMBO_BOX:
      ia_role_ = ROLE_SYSTEM_COMBOBOX;
      break;
    case AccessibilityNodeData::ROLE_DIV:
      role_name_ = L"div";
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case AccessibilityNodeData::ROLE_DEFINITION:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_PARAGRAPH;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_DESCRIPTION_LIST_DETAIL:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_PARAGRAPH;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_DESCRIPTION_LIST_TERM:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_DIALOG:
      ia_role_ = ROLE_SYSTEM_DIALOG;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_DISCLOSURE_TRIANGLE:
      ia_role_ = ROLE_SYSTEM_OUTLINEBUTTON;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_DOCUMENT:
    case AccessibilityNodeData::ROLE_ROOT_WEB_AREA:
    case AccessibilityNodeData::ROLE_WEB_AREA:
      ia_role_ = ROLE_SYSTEM_DOCUMENT;
      ia_state_ |= STATE_SYSTEM_READONLY;
      ia_state_ |= STATE_SYSTEM_FOCUSABLE;
      break;
    case AccessibilityNodeData::ROLE_EDITABLE_TEXT:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case AccessibilityNodeData::ROLE_FORM:
      role_name_ = L"form";
      ia2_role_ = IA2_ROLE_FORM;
      break;
    case AccessibilityNodeData::ROLE_FOOTER:
      ia_role_ = IA2_ROLE_FOOTER;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_GRID:
      ia_role_ = ROLE_SYSTEM_TABLE;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_GROUP: {
      string16 aria_role;
      GetStringAttribute(AccessibilityNodeData::ATTR_ROLE, &aria_role);
      if (aria_role == L"group" || html_tag == L"fieldset") {
        ia_role_ = ROLE_SYSTEM_GROUPING;
      } else if (html_tag == L"li") {
        ia_role_ = ROLE_SYSTEM_LISTITEM;
      } else {
        if (html_tag.empty())
          role_name_ = L"div";
        else
          role_name_ = html_tag;
        ia2_role_ = IA2_ROLE_SECTION;
      }
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    }
    case AccessibilityNodeData::ROLE_GROW_AREA:
      ia_role_ = ROLE_SYSTEM_GRIP;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_HEADING:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_HEADING;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_HORIZONTAL_RULE:
      ia_role_ = ROLE_SYSTEM_SEPARATOR;
      break;
    case AccessibilityNodeData::ROLE_IMAGE:
      ia_role_ = ROLE_SYSTEM_GRAPHIC;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_IMAGE_MAP:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_IMAGE_MAP;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_IMAGE_MAP_LINK:
      ia_role_ = ROLE_SYSTEM_LINK;
      ia_state_ |= STATE_SYSTEM_LINKED;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_LABEL:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_role_ = IA2_ROLE_LABEL;
      break;
    case AccessibilityNodeData::ROLE_LANDMARK_APPLICATION:
    case AccessibilityNodeData::ROLE_LANDMARK_BANNER:
    case AccessibilityNodeData::ROLE_LANDMARK_COMPLEMENTARY:
    case AccessibilityNodeData::ROLE_LANDMARK_CONTENTINFO:
    case AccessibilityNodeData::ROLE_LANDMARK_MAIN:
    case AccessibilityNodeData::ROLE_LANDMARK_NAVIGATION:
    case AccessibilityNodeData::ROLE_LANDMARK_SEARCH:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_LINK:
    case AccessibilityNodeData::ROLE_WEBCORE_LINK:
      ia_role_ = ROLE_SYSTEM_LINK;
      ia_state_ |= STATE_SYSTEM_LINKED;
      break;
    case AccessibilityNodeData::ROLE_LIST:
      ia_role_ = ROLE_SYSTEM_LIST;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_LISTBOX:
      ia_role_ = ROLE_SYSTEM_LIST;
      break;
    case AccessibilityNodeData::ROLE_LISTBOX_OPTION:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      if (ia_state_ & STATE_SYSTEM_SELECTABLE) {
        ia_state_ |= STATE_SYSTEM_FOCUSABLE;
        if (HasState(AccessibilityNodeData::STATE_FOCUSED))
          ia_state_ |= STATE_SYSTEM_FOCUSED;
      }
      break;
    case AccessibilityNodeData::ROLE_LIST_ITEM:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_LIST_MARKER:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_MATH:
      ia_role_ = ROLE_SYSTEM_EQUATION;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_MENU:
    case AccessibilityNodeData::ROLE_MENU_BUTTON:
      ia_role_ = ROLE_SYSTEM_MENUPOPUP;
      break;
    case AccessibilityNodeData::ROLE_MENU_BAR:
      ia_role_ = ROLE_SYSTEM_MENUBAR;
      break;
    case AccessibilityNodeData::ROLE_MENU_ITEM:
      ia_role_ = ROLE_SYSTEM_MENUITEM;
      break;
    case AccessibilityNodeData::ROLE_MENU_LIST_POPUP:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      break;
    case AccessibilityNodeData::ROLE_MENU_LIST_OPTION:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      if (ia_state_ & STATE_SYSTEM_SELECTABLE) {
        ia_state_ |= STATE_SYSTEM_FOCUSABLE;
        if (HasState(AccessibilityNodeData::STATE_FOCUSED))
          ia_state_ |= STATE_SYSTEM_FOCUSED;
      }
      break;
    case AccessibilityNodeData::ROLE_NOTE:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_NOTE;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_OUTLINE:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_PARAGRAPH:
      role_name_ = L"P";
      ia2_role_ = IA2_ROLE_PARAGRAPH;
      break;
    case AccessibilityNodeData::ROLE_POPUP_BUTTON:
      if (html_tag == L"select") {
        ia_role_ = ROLE_SYSTEM_COMBOBOX;
      } else {
        ia_role_ = ROLE_SYSTEM_BUTTONMENU;
      }
      break;
    case AccessibilityNodeData::ROLE_PROGRESS_INDICATOR:
      ia_role_ = ROLE_SYSTEM_PROGRESSBAR;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_RADIO_BUTTON:
      ia_role_ = ROLE_SYSTEM_RADIOBUTTON;
      break;
    case AccessibilityNodeData::ROLE_RADIO_GROUP:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case AccessibilityNodeData::ROLE_REGION:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_ROW:
      ia_role_ = ROLE_SYSTEM_ROW;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_ROW_HEADER:
      ia_role_ = ROLE_SYSTEM_ROWHEADER;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_RULER:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_RULER;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_SCROLLAREA:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SCROLL_PANE;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_SCROLLBAR:
      ia_role_ = ROLE_SYSTEM_SCROLLBAR;
      break;
    case AccessibilityNodeData::ROLE_SLIDER:
      ia_role_ = ROLE_SYSTEM_SLIDER;
      break;
    case AccessibilityNodeData::ROLE_SPIN_BUTTON:
      ia_role_ = ROLE_SYSTEM_SPINBUTTON;
      break;
    case AccessibilityNodeData::ROLE_SPIN_BUTTON_PART:
      ia_role_ = ROLE_SYSTEM_PUSHBUTTON;
      break;
    case AccessibilityNodeData::ROLE_SPLIT_GROUP:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SPLIT_PANE;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_ANNOTATION:
    case AccessibilityNodeData::ROLE_STATIC_TEXT:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_STATUS:
      ia_role_ = ROLE_SYSTEM_STATUSBAR;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_SPLITTER:
      ia_role_ = ROLE_SYSTEM_SEPARATOR;
      break;
    case AccessibilityNodeData::ROLE_SVG_ROOT:
      ia_role_ = ROLE_SYSTEM_GRAPHIC;
      break;
    case AccessibilityNodeData::ROLE_TAB:
      ia_role_ = ROLE_SYSTEM_PAGETAB;
      break;
    case AccessibilityNodeData::ROLE_TABLE: {
      string16 aria_role;
      GetStringAttribute(AccessibilityNodeData::ATTR_ROLE, &aria_role);
      if (aria_role == L"treegrid") {
        ia_role_ = ROLE_SYSTEM_OUTLINE;
      } else {
        ia_role_ = ROLE_SYSTEM_TABLE;
        ia_state_ |= STATE_SYSTEM_READONLY;
      }
      break;
    }
    case AccessibilityNodeData::ROLE_TABLE_HEADER_CONTAINER:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_TAB_GROUP_UNUSED:
      NOTREACHED();
      ia_role_ = ROLE_SYSTEM_PAGETABLIST;
      break;
    case AccessibilityNodeData::ROLE_TAB_LIST:
      ia_role_ = ROLE_SYSTEM_PAGETABLIST;
      break;
    case AccessibilityNodeData::ROLE_TAB_PANEL:
      ia_role_ = ROLE_SYSTEM_PROPERTYPAGE;
      break;
    case AccessibilityNodeData::ROLE_TOGGLE_BUTTON:
      ia_role_ = ROLE_SYSTEM_PUSHBUTTON;
      ia2_role_ = IA2_ROLE_TOGGLE_BUTTON;
      break;
    case AccessibilityNodeData::ROLE_TEXTAREA:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_MULTI_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      ia2_state_ |= IA2_STATE_SELECTABLE_TEXT;
      break;
    case AccessibilityNodeData::ROLE_TEXT_FIELD:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      ia2_state_ |= IA2_STATE_SELECTABLE_TEXT;
      break;
    case AccessibilityNodeData::ROLE_TIMER:
      ia_role_ = ROLE_SYSTEM_CLOCK;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_TOOLBAR:
      ia_role_ = ROLE_SYSTEM_TOOLBAR;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_TOOLTIP:
      ia_role_ = ROLE_SYSTEM_TOOLTIP;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_TREE:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_TREE_GRID:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_TREE_ITEM:
      ia_role_ = ROLE_SYSTEM_OUTLINEITEM;
      ia_state_ |= STATE_SYSTEM_READONLY;
      break;
    case AccessibilityNodeData::ROLE_WINDOW:
      ia_role_ = ROLE_SYSTEM_WINDOW;
      break;

    // TODO(dmazzoni): figure out the proper MSAA role for all of these.
    case AccessibilityNodeData::ROLE_BROWSER:
    case AccessibilityNodeData::ROLE_DIRECTORY:
    case AccessibilityNodeData::ROLE_DRAWER:
    case AccessibilityNodeData::ROLE_HELP_TAG:
    case AccessibilityNodeData::ROLE_IGNORED:
    case AccessibilityNodeData::ROLE_INCREMENTOR:
    case AccessibilityNodeData::ROLE_LOG:
    case AccessibilityNodeData::ROLE_MARQUEE:
    case AccessibilityNodeData::ROLE_MATTE:
    case AccessibilityNodeData::ROLE_PRESENTATIONAL:
    case AccessibilityNodeData::ROLE_RULER_MARKER:
    case AccessibilityNodeData::ROLE_SHEET:
    case AccessibilityNodeData::ROLE_SLIDER_THUMB:
    case AccessibilityNodeData::ROLE_SYSTEM_WIDE:
    case AccessibilityNodeData::ROLE_VALUE_INDICATOR:
    default:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      break;
  }

  // Compute the final value of READONLY for MSAA.
  //
  // We always set the READONLY state for elements that have the
  // aria-readonly attribute and for a few roles (in the switch above).
  // We clear the READONLY state on focusable controls and on a document.
  // Everything else, the majority of objects, do not have this state set.
  if (HasState(AccessibilityNodeData::STATE_FOCUSABLE) &&
      ia_role_ != ROLE_SYSTEM_DOCUMENT) {
    ia_state_ &= ~(STATE_SYSTEM_READONLY);
  }
  if (!HasState(AccessibilityNodeData::STATE_READONLY))
    ia_state_ &= ~(STATE_SYSTEM_READONLY);
  bool aria_readonly = false;
  GetBoolAttribute(AccessibilityNodeData::ATTR_ARIA_READONLY, &aria_readonly);
  if (aria_readonly)
    ia_state_ |= STATE_SYSTEM_READONLY;

  // The role should always be set.
  DCHECK(!role_name_.empty() || ia_role_);

  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!ia2_role_)
    ia2_role_ = ia_role_;
}

}  // namespace content
