// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACCESSIBILITY_WIN_H_
#define CHROME_BROWSER_BROWSER_ACCESSIBILITY_WIN_H_
#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include <map>
#include <vector>

#include "chrome/browser/browser_accessibility_manager_win.h"
#include "ia2_api_all.h"  // Generated
#include "webkit/glue/webaccessibility.h"

using webkit_glue::WebAccessibility;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibility
//
// Class implementing the MSAA IAccessible COM interface for the
// Browser-Renderer communication of MSAA information, providing accessibility
// to be used by screen readers and other assistive technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class ATL_NO_VTABLE BrowserAccessibility
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchImpl<IAccessible2, &IID_IAccessible2,
                         &LIBID_IAccessible2Lib>,
    public IAccessibleImage,
    public IAccessibleText,
    public IServiceProvider {
 public:
  BEGIN_COM_MAP(BrowserAccessibility)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible2)
    COM_INTERFACE_ENTRY2(IAccessible, IAccessible2)
    COM_INTERFACE_ENTRY(IAccessible2)
    COM_INTERFACE_ENTRY(IAccessibleImage)
    COM_INTERFACE_ENTRY(IAccessibleText)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()

  BrowserAccessibility();

  virtual ~BrowserAccessibility();

  // Initialize this object and mark it as active.
  void Initialize(BrowserAccessibilityManager* manager,
                  BrowserAccessibility* parent,
                  LONG child_id,
                  LONG index_in_parent,
                  const webkit_glue::WebAccessibility& src);

  // Add a child of this object.
  void AddChild(BrowserAccessibility* child);

  // Mark this object as inactive, and remove references to all children.
  // When no other clients hold any references to this object it will be
  // deleted, and in the meantime, calls to any methods will return E_FAIL.
  void InactivateTree();

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(BrowserAccessibility* ancestor);

  // Returns the parent of this object, or NULL if it's the BrowserAccessibility
  // root.
  BrowserAccessibility* GetParent();

  // Return the previous sibling of this object, or NULL if it's the first
  // child of its parent.
  BrowserAccessibility* GetPreviousSibling();

  // Return the next sibling of this object, or NULL if it's the last child
  // of its parent.
  BrowserAccessibility* GetNextSibling();

  // Replace a child BrowserAccessibility object. Used when updating the
  // accessibility tree.
  void ReplaceChild(
      const BrowserAccessibility* old_acc, BrowserAccessibility* new_acc);

  // Accessors
  LONG child_id() const { return child_id_; }
  int32 renderer_id() const { return renderer_id_; }
  LONG index_in_parent() const { return index_in_parent_; }

  // Add one to the reference count and return the same object. Always
  // use this method when returning a BrowserAccessibility object as
  // an output parameter to a COM interface, never use it otherwise.
  BrowserAccessibility* NewReference();

  //
  // IAccessible methods.
  //

  // Performs the default action on a given object.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id);

  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child);

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left,
                           LONG* y_top,
                           LONG* width,
                           LONG* height,
                           VARIANT var_id);

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end);

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child);

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count);

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id, BSTR* default_action);

  // Retrieves the object's description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc);

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child);

  // Retrieves the help information associated with the object.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help);

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id, BSTR* access_key);

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name);

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent);

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role);

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state);

  // Returns the value associated with the object.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value);

  // Make an object take focus or extend the selection.
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id);

  STDMETHODIMP get_accHelpTopic(BSTR* help_file,
                                VARIANT var_id,
                                LONG* topic_id);

  STDMETHODIMP get_accSelection(VARIANT* selected);

  // Deprecated methods, not implemented.
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name) {
    return E_NOTIMPL;
  }
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR put_val) {
    return E_NOTIMPL;
  }

  //
  // IAccessible2 methods.
  //

  // Returns role from a longer list of possible roles.
  STDMETHODIMP role(LONG* role);

  // Returns the state bitmask from a larger set of possible states.
  STDMETHODIMP get_states(AccessibleStates* states);

  // Returns the attributes specific to this IAccessible2 object,
  // such as a cell's formula.
  STDMETHODIMP get_attributes(BSTR* attributes);

  // Get the unique ID of this object so that the client knows if it's
  // been encountered previously.
  STDMETHODIMP get_uniqueID(LONG* unique_id);

  // Get the window handle of the enclosing window.
  STDMETHODIMP get_windowHandle(HWND* window_handle);

  // Get this object's index in its parent object.
  STDMETHODIMP get_indexInParent(LONG* index_in_parent);

  // IAccessible2 methods not implemented.
  STDMETHODIMP get_extendedRole(BSTR* extended_role) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_nRelations(LONG* n_relations) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_relation(LONG relation_index,
                            IAccessibleRelation** relation) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_relations(LONG max_relations,
                             IAccessibleRelation** relations,
                             LONG* n_relations) {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollTo(enum IA2ScrollType scroll_type) {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollToPoint(enum IA2CoordinateType coordinate_type,
                             LONG x,
                             LONG y) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_groupPosition(LONG* group_level,
                                 LONG* similar_items_in_group,
                                 LONG* position_in_group) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_localizedExtendedRole(BSTR* localized_extended_role) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_nExtendedStates(LONG* n_extended_states) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_extendedStates(LONG max_extended_states,
                                  BSTR** extended_states,
                                  LONG* n_extended_states) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_localizedExtendedStates(LONG max_localized_extended_states,
                                           BSTR** localized_extended_states,
                                           LONG* n_localized_extended_states) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_locale(IA2Locale* locale) {
    return E_NOTIMPL;
  }

  //
  // IAccessibleImage methods.
  //

  STDMETHODIMP get_description(BSTR* description);

  STDMETHODIMP get_imagePosition(enum IA2CoordinateType coordinate_type,
                                 long* x, long* y);

  STDMETHODIMP get_imageSize(long* height, long* width);

  //
  // IAccessibleText methods.
  //

  STDMETHODIMP get_nCharacters(long* n_characters);

  STDMETHODIMP get_text(long start_offset, long end_offset, BSTR* text);

  STDMETHODIMP get_caretOffset(long* offset);

  // IAccessibleText methods not implemented.
  STDMETHODIMP addSelection(long start_offset, long end_offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_attributes(long offset, long* start_offset, long* end_offset,
                              BSTR* text_attributes) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_characterExtents(long offset,
                                    enum IA2CoordinateType coord_type,
                                    long* x, long* y,
                                    long* width, long* height) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_nSelections(long* n_selections) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_offsetAtPoint(long x, long y,
                             enum IA2CoordinateType coord_type,
                             long* offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_selection(long selection_index,
                         long* start_offset,
                         long* end_offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_textBeforeOffset(long offset,
                                enum IA2TextBoundaryType boundary_type,
                                long* start_offset, long* end_offset,
                                BSTR* text) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_textAfterOffset(long offset,
                               enum IA2TextBoundaryType boundary_type,
                               long* start_offset, long* end_offset,
                               BSTR* text) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_textAtOffset(long offset,
                            enum IA2TextBoundaryType boundary_type,
                            long* start_offset, long* end_offset,
                            BSTR* text) {
    return E_NOTIMPL;
  }
  STDMETHODIMP removeSelection(long selection_index) {
    return E_NOTIMPL;
  }
  STDMETHODIMP setCaretOffset(long offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP setSelection(long selection_index,
                            long start_offset,
                            long end_offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollSubstringTo(long start_index,
                                 long end_index,
                                 enum IA2ScrollType scroll_type) {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollSubstringToPoint(long start_index, long end_index,
                                      enum IA2CoordinateType coordinate_type,
                                      long x, long y) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_newText(IA2TextSegment* new_text) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_oldText(IA2TextSegment* old_text) {
    return E_NOTIMPL;
  }

  //
  // IServiceProvider methods.
  //

  STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void** object);

  //
  // CComObjectRootEx methods.
  //

  HRESULT WINAPI InternalQueryInterface(void* this_ptr,
                                        const _ATL_INTMAP_ENTRY* entries,
                                        REFIID iid,
                                        void** object);

 private:
  // Many MSAA methods take a var_id parameter indicating that the operation
  // should be performed on a particular child ID, rather than this object.
  // This method tries to figure out the target object from |var_id| and
  // returns a pointer to the target object if it exists, otherwise NULL.
  // Does not return a new reference.
  BrowserAccessibility* GetTargetFromChildID(const VARIANT& var_id);

  // Initialize the role and state metadata from the role enum and state
  // bitmasks defined in webkit/glue/webaccessibility.h.
  void InitRoleAndState(LONG web_accessibility_role,
                        LONG web_accessibility_state);

  // Return true if this attribute is in the attributes map.
  bool HasAttribute(WebAccessibility::Attribute attribute);

  // Retrieve the string value of an attribute from the attribute map and
  // returns true if found.
  bool GetAttribute(WebAccessibility::Attribute attribute, string16* value);

  // The manager of this tree of accessibility objects; needed for
  // global operations like focus tracking.
  BrowserAccessibilityManager* manager_;
  // The parent of this object, may be NULL if we're the root object.
  BrowserAccessibility* parent_;
  // The ID of this object; globally unique within the browser process.
  LONG child_id_;
  // The index of this within its parent object.
  LONG index_in_parent_;
  // The ID of this object in the renderer process.
  int32 renderer_id_;

  // The children of this object.
  std::vector<BrowserAccessibility*> children_;

  // Accessibility metadata from the renderer, used to respond to MSAA
  // events.
  string16 name_;
  string16 value_;
  std::map<int32, string16> attributes_;

  LONG role_;
  LONG state_;
  string16 role_name_;
  LONG ia2_role_;
  LONG ia2_state_;
  WebKit::WebRect location_;

  // COM objects are reference-counted. When we're done with this object
  // and it's removed from our accessibility tree, a client may still be
  // holding onto a pointer to this object, so we mark it as inactive
  // so that calls to any of this object's methods immediately return
  // failure.
  bool instance_active_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibility);
};

#endif  // CHROME_BROWSER_BROWSER_ACCESSIBILITY_WIN_H_
