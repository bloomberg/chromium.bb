// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include <vector>

#include "chrome/browser/accessibility/browser_accessibility.h"
#include "ia2_api_all.h"  // Generated
#include "ISimpleDOMDocument.h"  // Generated
#include "ISimpleDOMNode.h"  // Generated
#include "ISimpleDOMText.h"  // Generated
#include "webkit/glue/webaccessibility.h"

class BrowserAccessibilityManagerWin;

using webkit_glue::WebAccessibility;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibilityWin
//
// Class implementing the windows accessible interface for the Browser-Renderer
// communication of accessibility information, providing accessibility
// to be used by screen readers and other assistive technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class BrowserAccessibilityWin
    : public BrowserAccessibility,
      public CComObjectRootEx<CComMultiThreadModel>,
      public IDispatchImpl<IAccessible2, &IID_IAccessible2,
                           &LIBID_IAccessible2Lib>,
      public IAccessibleImage,
      public IAccessibleText,
      public IServiceProvider,
      public ISimpleDOMDocument,
      public ISimpleDOMNode,
      public ISimpleDOMText {
 public:
  BEGIN_COM_MAP(BrowserAccessibilityWin)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible2)
    COM_INTERFACE_ENTRY2(IAccessible, IAccessible2)
    COM_INTERFACE_ENTRY(IAccessible2)
    COM_INTERFACE_ENTRY(IAccessibleImage)
    COM_INTERFACE_ENTRY(IAccessibleText)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(ISimpleDOMDocument)
    COM_INTERFACE_ENTRY(ISimpleDOMNode)
    COM_INTERFACE_ENTRY(ISimpleDOMText)
  END_COM_MAP()

  BrowserAccessibilityWin();

  virtual ~BrowserAccessibilityWin();

  //
  // BrowserAccessibility methods.
  //
  virtual void Initialize();
  virtual void NativeAddReference();
  virtual void NativeReleaseReference();

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
                                 LONG* x, LONG* y);

  STDMETHODIMP get_imageSize(LONG* height, LONG* width);

  //
  // IAccessibleText methods.
  //

  STDMETHODIMP get_nCharacters(LONG* n_characters);

  STDMETHODIMP get_caretOffset(LONG* offset);

  STDMETHODIMP get_nSelections(LONG* n_selections);

  STDMETHODIMP get_selection(LONG selection_index,
                             LONG* start_offset,
                             LONG* end_offset);

  STDMETHODIMP get_text(LONG start_offset, LONG end_offset, BSTR* text);

  STDMETHODIMP get_textAtOffset(LONG offset,
                                enum IA2TextBoundaryType boundary_type,
                                LONG* start_offset, LONG* end_offset,
                                BSTR* text);

  STDMETHODIMP get_textBeforeOffset(LONG offset,
                                    enum IA2TextBoundaryType boundary_type,
                                    LONG* start_offset, LONG* end_offset,
                                    BSTR* text);

  STDMETHODIMP get_textAfterOffset(LONG offset,
                                   enum IA2TextBoundaryType boundary_type,
                                   LONG* start_offset, LONG* end_offset,
                                   BSTR* text);

  // IAccessibleText methods not implemented.
  STDMETHODIMP addSelection(LONG start_offset, LONG end_offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_attributes(LONG offset, LONG* start_offset, LONG* end_offset,
                              BSTR* text_attributes) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_characterExtents(LONG offset,
                                    enum IA2CoordinateType coord_type,
                                    LONG* x, LONG* y,
                                    LONG* width, LONG* height) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_offsetAtPoint(LONG x, LONG y,
                                 enum IA2CoordinateType coord_type,
                                 LONG* offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP removeSelection(LONG selection_index) {
    return E_NOTIMPL;
  }
  STDMETHODIMP setCaretOffset(LONG offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP setSelection(LONG selection_index,
                            LONG start_offset,
                            LONG end_offset) {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollSubstringTo(LONG start_index,
                                 LONG end_index,
                                 enum IA2ScrollType scroll_type) {
    return E_NOTIMPL;
  }
  STDMETHODIMP scrollSubstringToPoint(LONG start_index, LONG end_index,
                                      enum IA2CoordinateType coordinate_type,
                                      LONG x, LONG y) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_newText(IA2TextSegment* new_text) {
    return E_NOTIMPL;
  }
  STDMETHODIMP get_oldText(IA2TextSegment* old_text) {
    return E_NOTIMPL;
  }

  //
  // ISimpleDOMDocument methods.
  //

  STDMETHODIMP get_URL(BSTR* url);

  STDMETHODIMP get_title(BSTR* title);

  STDMETHODIMP get_mimeType(BSTR* mime_type);

  STDMETHODIMP get_docType(BSTR* doc_type);

  STDMETHODIMP get_nameSpaceURIForID(
      short name_space_id, BSTR *name_space_uri) {
    return E_NOTIMPL;
  }
  STDMETHODIMP put_alternateViewMediaTypes(
      BSTR *comma_separated_media_types) {
    return E_NOTIMPL;
  }

  //
  // ISimpleDOMNode methods.
  //

  STDMETHODIMP get_nodeInfo(
      BSTR* node_name,
      short* name_space_id,
      BSTR* node_value,
      unsigned int* num_children,
      unsigned int* unique_id,
      unsigned short* node_type);

  STDMETHODIMP get_attributes(
      unsigned short max_attribs,
      BSTR* attrib_names,
      short* name_space_id,
      BSTR* attrib_values,
      unsigned short* num_attribs);

  STDMETHODIMP get_attributesForNames(
      unsigned short num_attribs,
      BSTR* attrib_names,
      short* name_space_id,
      BSTR* attrib_values);

  STDMETHODIMP get_computedStyle(
      unsigned short max_style_properties,
      boolean use_alternate_view,
      BSTR *style_properties,
      BSTR *style_values,
      unsigned short *num_style_properties);

  STDMETHODIMP get_computedStyleForProperties(
      unsigned short num_style_properties,
      boolean use_alternate_view,
      BSTR* style_properties,
      BSTR* style_values);

  STDMETHODIMP scrollTo(boolean placeTopLeft);

  STDMETHODIMP get_parentNode(ISimpleDOMNode** node);

  STDMETHODIMP get_firstChild(ISimpleDOMNode** node);

  STDMETHODIMP get_lastChild(ISimpleDOMNode** node);

  STDMETHODIMP get_previousSibling(ISimpleDOMNode** node);

  STDMETHODIMP get_nextSibling(ISimpleDOMNode** node);

  STDMETHODIMP get_childAt(
      unsigned int child_index,
      ISimpleDOMNode** node);

  STDMETHODIMP get_innerHTML(BSTR* innerHTML) {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_localInterface(void** local_interface) {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_language(BSTR* language) {
    return E_NOTIMPL;
  }

  //
  // ISimpleDOMText methods.
  //

  STDMETHODIMP get_domText(BSTR* dom_text);

  STDMETHODIMP get_clippedSubstringBounds(
      unsigned int start_index,
      unsigned int end_index,
      int* x,
      int* y,
      int* width,
      int* height) {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_unclippedSubstringBounds(
      unsigned int start_index,
      unsigned int end_index,
      int* x,
      int* y,
      int* width,
      int* height) {
    return E_NOTIMPL;
  }

  STDMETHODIMP scrollToSubstring(
      unsigned int start_index,
      unsigned int end_index)  {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_fontFamily(BSTR *font_family)  {
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
  // Add one to the reference count and return the same object. Always
  // use this method when returning a BrowserAccessibilityWin object as
  // an output parameter to a COM interface, never use it otherwise.
  BrowserAccessibilityWin* NewReference();

  // Many MSAA methods take a var_id parameter indicating that the operation
  // should be performed on a particular child ID, rather than this object.
  // This method tries to figure out the target object from |var_id| and
  // returns a pointer to the target object if it exists, otherwise NULL.
  // Does not return a new reference.
  BrowserAccessibilityWin* GetTargetFromChildID(const VARIANT& var_id);

  // Initialize the role and state metadata from the role enum and state
  // bitmasks defined in webkit/glue/webaccessibility.h.
  void InitRoleAndState();

  // Retrieve the string value of an attribute from the attribute map and
  // if found and nonempty, allocate a new BSTR (with SysAllocString)
  // and return S_OK. If not found or empty, return S_FALSE.
  HRESULT GetAttributeAsBstr(
      WebAccessibility::Attribute attribute, BSTR* value_bstr);

  // Escape a string like it would be escaped for a URL or HTML form.
  string16 Escape(const string16& str);

  // Get the text of this node for the purposes of IAccessibleText - it may
  // be the name, it may be the value, etc. depending on the role.
  const string16& TextForIAccessibleText();

  // Search forwards (direction == 1) or backwards (direction == -1) from
  // the given offset until the given IAccessible2 boundary (like word,
  // sentence) is found, and return its offset.
  LONG FindBoundary(const string16& text,
                    IA2TextBoundaryType boundary,
                    LONG start_offset,
                    LONG direction);

  // IAccessible role and state.
  int32 ia_role_;
  int32 ia_state_;

  // IAccessible2 role and state.
  int32 ia2_role_;
  int32 ia2_state_;

  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityWin);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
