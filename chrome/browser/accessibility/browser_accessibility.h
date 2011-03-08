// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "webkit/glue/webaccessibility.h"

class BrowserAccessibilityManager;
#if defined(OS_MACOSX) && __OBJC__
@class BrowserAccessibilityCocoa;
#elif defined(OS_WIN)
class BrowserAccessibilityWin;
#endif

using webkit_glue::WebAccessibility;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibility
//
// Class implementing the cross platform interface for the Browser-Renderer
// communication of accessibility information, providing accessibility
// to be used by screen readers and other assistive technology (AT).
//
// An implementation for each platform handles platform specific accessibility
// APIs.
//
////////////////////////////////////////////////////////////////////////////////
class BrowserAccessibility {
 public:
  // Creates a platform specific BrowserAccessibility. Ownership passes to the
  // caller.
  static BrowserAccessibility* Create();

  virtual ~BrowserAccessibility();

  // Perform platform specific initialization. This can be called multiple times
  // during the lifetime of this instance after the members of this base object
  // have been reset with new values from the renderer process.
  virtual void Initialize();

  // Replace a child object. Used when updating the accessibility tree.
  virtual void ReplaceChild(
      BrowserAccessibility* old_acc,
      BrowserAccessibility* new_acc);

  // Initialize this object, reading attributes from |src|. Does not
  // recurse into children of |src| and build the whole subtree.
  void Initialize(BrowserAccessibilityManager* manager,
                  BrowserAccessibility* parent,
                  int32 child_id,
                  int32 index_in_parent,
                  const WebAccessibility& src);

  // Add a child of this object.
  void AddChild(BrowserAccessibility* child);

  // Detach all descendants of this subtree and push all of the node pointers,
  // including this node, onto the end of |nodes|.
  void DetachTree(std::vector<BrowserAccessibility*>* nodes);

  // Update the parent and index in parent if this node has been moved.
  void UpdateParent(BrowserAccessibility* parent, int index_in_parent);

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(BrowserAccessibility* ancestor);

  // Returns the parent of this object, or NULL if it's the root.
  BrowserAccessibility* GetParent();

  // Returns the number of children of this object.
  uint32 GetChildCount();

  // Return a pointer to the child with the given index.
  BrowserAccessibility* GetChild(uint32 child_index);

  // Return the previous sibling of this object, or NULL if it's the first
  // child of its parent.
  BrowserAccessibility* GetPreviousSibling();

  // Return the next sibling of this object, or NULL if it's the last child
  // of its parent.
  BrowserAccessibility* GetNextSibling();

  // Returns the bounds of this object in screen coordinates.
  gfx::Rect GetBoundsRect();

  // Returns the deepest descendant that contains the specified point.
  BrowserAccessibility* BrowserAccessibilityForPoint(const gfx::Point& point);

  //
  // Reference counting
  //
  // Each object has an internal reference count and many platform
  // implementations may also use native reference counting.
  //
  // The internal reference counting is used because sometimes
  // multiple references to the same object exist temporarily during
  // an update. When the internal reference count reaches zero,
  // NativeReleaseReference is called.
  //
  // Native reference counting is used on some platforms because the
  // operating system may hold onto a reference to a BrowserAccessibility
  // object even after we're through with it. On these platforms, when
  // the internal reference count reaches zero, instance_active is set
  // to zero, and all queries on this object should return failure.
  // The object isn't actually deleted until the operating system releases
  // all of its references.
  //

  // Increment this node's internal reference count.
  virtual void InternalAddReference();

  // Decrement this node's internal reference count. If the reference count
  // reaches zero, call NativeReleaseReference().
  virtual void InternalReleaseReference(bool recursive);

  // Subclasses should override this to support platform reference counting.
  virtual void NativeAddReference() { }

  // Subclasses should override this to support platform reference counting.
  virtual void NativeReleaseReference() { delete this; }

  //
  // Accessors
  //

  const std::map<int32, string16>& attributes() const { return attributes_; }
  int32 child_id() const { return child_id_; }
  const std::vector<BrowserAccessibility*>& children() const {
    return children_;
  }
  const std::vector<std::pair<string16, string16> >& html_attributes() const {
    return html_attributes_;
  }
  int32 index_in_parent() const { return index_in_parent_; }
  WebKit::WebRect location() const { return location_; }
  BrowserAccessibilityManager* manager() const { return manager_; }
  const string16& name() const { return name_; }
  int32 renderer_id() const { return renderer_id_; }
  int32 role() const { return role_; }
  const string16& role_name() const { return role_name_; }
  int32 state() const { return state_; }
  const string16& value() const { return value_; }
  bool instance_active() const { return instance_active_; }
  int32 ref_count() const { return ref_count_; }

#if defined(OS_MACOSX) && __OBJC__
  BrowserAccessibilityCocoa* toBrowserAccessibilityCocoa();
#elif defined(OS_WIN)
  BrowserAccessibilityWin* toBrowserAccessibilityWin();
#endif

  // BrowserAccessibilityCocoa needs access to these methods.
  // Return true if this attribute is in the attributes map.
  bool HasAttribute(WebAccessibility::Attribute attribute);

  // Retrieve the string value of an attribute from the attribute map and
  // returns true if found.
  bool GetAttribute(WebAccessibility::Attribute attribute, string16* value);

  // Retrieve the value of an attribute from the attribute map and
  // if found and nonempty, try to convert it to an integer.
  // Returns true only if both the attribute was found and it was successfully
  // converted to an integer.
  bool GetAttributeAsInt(
      WebAccessibility::Attribute attribute, int* value_int);

 protected:
  BrowserAccessibility();

  // The manager of this tree of accessibility objects; needed for
  // global operations like focus tracking.
  BrowserAccessibilityManager* manager_;

  // The parent of this object, may be NULL if we're the root object.
  BrowserAccessibility* parent_;

  // The ID of this object; globally unique within the browser process.
  int32 child_id_;

  // The index of this within its parent object.
  int32 index_in_parent_;

  // The ID of this object in the renderer process.
  int32 renderer_id_;

  // The children of this object.
  std::vector<BrowserAccessibility*> children_;

  // The number of internal references to this object.
  int32 ref_count_;

  // Accessibility metadata from the renderer
  string16 name_;
  string16 value_;
  std::map<int32, string16> attributes_;
  std::vector<std::pair<string16, string16> > html_attributes_;
  int32 role_;
  int32 state_;
  string16 role_name_;
  WebKit::WebRect location_;
  std::vector<int32> indirect_child_ids_;

  // BrowserAccessibility objects are reference-counted on some platforms.
  // When we're done with this object and it's removed from our accessibility
  // tree, a client may still be holding onto a pointer to this object, so
  // we mark it as inactive so that calls to any of this object's methods
  // immediately return failure.
  bool instance_active_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibility);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
