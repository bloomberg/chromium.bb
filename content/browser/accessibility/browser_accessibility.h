// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/accessibility/ax_node_data.h"

#if defined(OS_MACOSX) && __OBJC__
@class BrowserAccessibilityCocoa;
#endif

namespace content {
class BrowserAccessibilityManager;
#if defined(OS_WIN)
class BrowserAccessibilityWin;
#elif defined(TOOLKIT_GTK)
class BrowserAccessibilityGtk;
#endif

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
class CONTENT_EXPORT BrowserAccessibility {
 public:
  // Creates a platform specific BrowserAccessibility. Ownership passes to the
  // caller.
  static BrowserAccessibility* Create();

  virtual ~BrowserAccessibility();

  // Detach all descendants of this subtree and push all of the node pointers,
  // including this node, onto the end of |nodes|.
  virtual void DetachTree(std::vector<BrowserAccessibility*>* nodes);

  // Perform platform-specific initialization. This can be called multiple times
  // during the lifetime of this instance after the members of this base object
  // have been reset with new values from the renderer process.
  // Child dependent initialization can be done here.
  virtual void PostInitialize() {}

  // Returns true if this is a native platform-specific object, vs a
  // cross-platform generic object.
  virtual bool IsNative() const;

  // Initialize the tree structure of this object.
  void InitializeTreeStructure(
      BrowserAccessibilityManager* manager,
      BrowserAccessibility* parent,
      int32 id,
      int32 index_in_parent);

  // Initialize this object's data.
  void InitializeData(const ui::AXNodeData& src);

  virtual void SwapChildren(std::vector<BrowserAccessibility*>& children);

  // Update the parent and index in parent if this node has been moved.
  void UpdateParent(BrowserAccessibility* parent, int index_in_parent);

  // Update this node's location, leaving everything else the same.
  virtual void SetLocation(const gfx::Rect& new_location);

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(BrowserAccessibility* ancestor);

  // Returns true if this is a leaf node on this platform, meaning any
  // children should not be exposed to this platform's native accessibility
  // layer. Each platform subclass should implement this itself.
  // The definition of a leaf may vary depending on the platform,
  // but a leaf node should never have children that are focusable or
  // that might send notifications.
  virtual bool PlatformIsLeaf() const;

  // Returns the number of children of this object, or 0 if PlatformIsLeaf()
  // returns true.
  uint32 PlatformChildCount() const;

  // Return a pointer to the child at the given index, or NULL for an
  // invalid index. Returns NULL if PlatformIsLeaf() returns true.
  BrowserAccessibility* PlatformGetChild(uint32 child_index) const;

  // Return the previous sibling of this object, or NULL if it's the first
  // child of its parent.
  BrowserAccessibility* GetPreviousSibling();

  // Return the next sibling of this object, or NULL if it's the last child
  // of its parent.
  BrowserAccessibility* GetNextSibling();

  // Returns the bounds of this object in coordinates relative to the
  // top-left corner of the overall web area.
  gfx::Rect GetLocalBoundsRect() const;

  // Returns the bounds of this object in screen coordinates.
  gfx::Rect GetGlobalBoundsRect() const;

  // Returns the bounds of the given range in coordinates relative to the
  // top-left corner of the overall web area. Only valid when the
  // role is WebAXRoleStaticText.
  gfx::Rect GetLocalBoundsForRange(int start, int len) const;

  // Same as GetLocalBoundsForRange, in screen coordinates. Only valid when
  // the role is WebAXRoleStaticText.
  gfx::Rect GetGlobalBoundsForRange(int start, int len) const;

  // Returns the deepest descendant that contains the specified point
  // (in global screen coordinates).
  BrowserAccessibility* BrowserAccessibilityForPoint(const gfx::Point& point);

  // Marks this object for deletion, releases our reference to it, and
  // recursively calls Destroy() on its children.  May not delete
  // immediately due to reference counting.
  //
  // Reference counting is used on some platforms because the
  // operating system may hold onto a reference to a BrowserAccessibility
  // object even after we're through with it. When a BrowserAccessibility
  // has had Destroy() called but its reference count is not yet zero,
  // queries on this object return failure
  virtual void Destroy();

  // Subclasses should override this to support platform reference counting.
  virtual void NativeAddReference() { }

  // Subclasses should override this to support platform reference counting.
  virtual void NativeReleaseReference();

  //
  // Accessors
  //

  BrowserAccessibilityManager* manager() const { return manager_; }
  bool instance_active() const { return instance_active_; }
  const std::string& name() const { return name_; }
  const std::string& value() const { return value_; }
  void set_name(const std::string& name) { name_ = name; }
  void set_value(const std::string& value) { value_ = value; }

  std::vector<BrowserAccessibility*>& deprecated_children() {
    return deprecated_children_;
  }

  // These access the internal accessibility tree, which doesn't necessarily
  // reflect the accessibility tree that should be exposed on each platform.
  // Use PlatformChildCount and PlatformGetChild to implement platform
  // accessibility APIs.
  uint32 InternalChildCount() const { return deprecated_children_.size(); }
  BrowserAccessibility* InternalGetChild(uint32 child_index) const {
    return deprecated_children_[child_index];
  }

  BrowserAccessibility* GetParent() const { return deprecated_parent_; }
  int32 GetIndexInParent() const { return deprecated_index_in_parent_; }

  int32 GetId() const { return data_.id; }
  gfx::Rect GetLocation() const { return data_.location; }
  int32 GetRole() const { return data_.role; }
  int32 GetState() const { return data_.state; }
  const std::vector<std::pair<std::string, std::string> >& GetHtmlAttributes()
      const {
    return data_.html_attributes;
  }

#if defined(OS_MACOSX) && __OBJC__
  BrowserAccessibilityCocoa* ToBrowserAccessibilityCocoa();
#elif defined(OS_WIN)
  BrowserAccessibilityWin* ToBrowserAccessibilityWin();
#elif defined(TOOLKIT_GTK)
  BrowserAccessibilityGtk* ToBrowserAccessibilityGtk();
#endif

  // Accessing accessibility attributes:
  //
  // There are dozens of possible attributes for an accessibility node,
  // but only a few tend to apply to any one object, so we store them
  // in sparse arrays of <attribute id, attribute value> pairs, organized
  // by type (bool, int, float, string, int list).
  //
  // There are three accessors for each type of attribute: one that returns
  // true if the attribute is present and false if not, one that takes a
  // pointer argument and returns true if the attribute is present (if you
  // need to distinguish between the default value and a missing attribute),
  // and another that returns the default value for that type if the
  // attribute is not present. In addition, strings can be returned as
  // either std::string or base::string16, for convenience.

  bool HasBoolAttribute(ui::AXBoolAttribute attr) const;
  bool GetBoolAttribute(ui::AXBoolAttribute attr) const;
  bool GetBoolAttribute(ui::AXBoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(ui::AXFloatAttribute attr) const;
  float GetFloatAttribute(ui::AXFloatAttribute attr) const;
  bool GetFloatAttribute(ui::AXFloatAttribute attr, float* value) const;

  bool HasIntAttribute(ui::AXIntAttribute attribute) const;
  int GetIntAttribute(ui::AXIntAttribute attribute) const;
  bool GetIntAttribute(ui::AXIntAttribute attribute, int* value) const;

  bool HasStringAttribute(
      ui::AXStringAttribute attribute) const;
  const std::string& GetStringAttribute(ui::AXStringAttribute attribute) const;
  bool GetStringAttribute(ui::AXStringAttribute attribute,
                          std::string* value) const;

  bool GetString16Attribute(ui::AXStringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(
      ui::AXStringAttribute attribute) const;

  bool HasIntListAttribute(ui::AXIntListAttribute attribute) const;
  const std::vector<int32>& GetIntListAttribute(
      ui::AXIntListAttribute attribute) const;
  bool GetIntListAttribute(ui::AXIntListAttribute attribute,
                           std::vector<int32>* value) const;

  void SetStringAttribute(ui::AXStringAttribute attribute,
                          const std::string& value);

  // Retrieve the value of a html attribute from the attribute map and
  // returns true if found.
  bool GetHtmlAttribute(const char* attr, base::string16* value) const;
  bool GetHtmlAttribute(const char* attr, std::string* value) const;

  // Utility method to handle special cases for ARIA booleans, tristates and
  // booleans which have a "mixed" state.
  //
  // Warning: the term "Tristate" is used loosely by the spec and here,
  // as some attributes support a 4th state.
  //
  // The following attributes are appropriate to use with this method:
  // aria-selected  (selectable)
  // aria-grabbed   (grabbable)
  // aria-expanded  (expandable)
  // aria-pressed   (toggleable/pressable) -- supports 4th "mixed" state
  // aria-checked   (checkable) -- supports 4th "mixed state"
  bool GetAriaTristate(const char* attr_name,
                       bool* is_defined,
                       bool* is_mixed) const;

  // Returns true if the bit corresponding to the given state enum is 1.
  bool HasState(ui::AXState state_enum) const;

  // Returns true if this node is an editable text field of any kind.
  bool IsEditableText() const;

  // Append the text from this node and its children.
  std::string GetTextRecursive() const;

 protected:
  // Perform platform specific initialization. This can be called multiple times
  // during the lifetime of this instance after the members of this base object
  // have been reset with new values from the renderer process.
  // Perform child independent initialization in this method.
  virtual void PreInitialize() {}

  BrowserAccessibility();

  // The manager of this tree of accessibility objects; needed for
  // global operations like focus tracking.
  BrowserAccessibilityManager* manager_;

  // The parent of this object, may be NULL if we're the root object.
  BrowserAccessibility* deprecated_parent_;

 private:
  // Return the sum of the lengths of all static text descendants,
  // including this object if it's static text.
  int GetStaticTextLenRecursive() const;

  // The index of this within its parent object.
  int32 deprecated_index_in_parent_;

  // The children of this object.
  std::vector<BrowserAccessibility*> deprecated_children_;

  // Accessibility metadata from the renderer
  std::string name_;
  std::string value_;
  ui::AXNodeData data_;

  // BrowserAccessibility objects are reference-counted on some platforms.
  // When we're done with this object and it's removed from our accessibility
  // tree, a client may still be holding onto a pointer to this object, so
  // we mark it as inactive so that calls to any of this object's methods
  // immediately return failure.
  bool instance_active_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibility);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
