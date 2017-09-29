// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_

#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_flags.h"
#include "content/browser/accessibility/ax_platform_position.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

// Set PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL if this platform has
// a platform-specific subclass of BrowserAccessibility and
// BrowserAccessibilityManager.
#undef PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL

#if defined(OS_WIN)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if defined(OS_MACOSX)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if defined(OS_ANDROID) && !defined(USE_AURA)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if BUILDFLAG(USE_ATK)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if defined(OS_MACOSX) && __OBJC__
@class BrowserAccessibilityCocoa;
#endif

namespace content {
class BrowserAccessibilityManager;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibility
//
// A BrowserAccessibility object represents one node in the accessibility
// tree on the browser side. It exactly corresponds to one WebAXObject from
// Blink. It's owned by a BrowserAccessibilityManager.
//
// There are subclasses of BrowserAccessibility for each platform where
// we implement native accessibility APIs. This base class is used occasionally
// for tests.
//
////////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT BrowserAccessibility : public ui::AXPlatformNodeDelegate {
 public:
  // Creates a platform specific BrowserAccessibility. Ownership passes to the
  // caller.
  static BrowserAccessibility* Create();

  virtual ~BrowserAccessibility();

  // Called only once, immediately after construction. The constructor doesn't
  // take any arguments because in the Windows subclass we use a special
  // function to construct a COM object.
  virtual void Init(BrowserAccessibilityManager* manager, ui::AXNode* node);

  // Called after the object is first initialized and again every time
  // its data changes.
  virtual void OnDataChanged() {}

  virtual void OnSubtreeWillBeDeleted() {}

  // Called when the location changed.
  virtual void OnLocationChanged() {}

  // This is called when the platform-specific attributes for a node need
  // to be recomputed, which may involve firing native events, due to a
  // change other than an update from OnAccessibilityEvents.
  virtual void UpdatePlatformAttributes() {}

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(const BrowserAccessibility* ancestor) const;

  bool IsDocument() const;

  bool IsEditField() const;

  // Returns true if this object is used only for representing text.
  bool IsTextOnlyObject() const;

  bool IsLineBreakObject() const;

  // Returns true if this is a leaf node on this platform, meaning any
  // children should not be exposed to this platform's native accessibility
  // layer. Each platform subclass should implement this itself.
  // The definition of a leaf may vary depending on the platform,
  // but a leaf node should never have children that are focusable or
  // that might send notifications.
  virtual bool PlatformIsLeaf() const;

  // Returns the number of children of this object, or 0 if PlatformIsLeaf()
  // returns true.
  uint32_t PlatformChildCount() const;

  // Return a pointer to the child at the given index, or NULL for an
  // invalid index. Returns NULL if PlatformIsLeaf() returns true.
  BrowserAccessibility* PlatformGetChild(uint32_t child_index) const;

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to the
  // platform.
  bool PlatformIsChildOfLeaf() const;

  // If this object is exposed to the platform, returns this object. Otherwise,
  // returns the platform leaf under which this object is found.
  BrowserAccessibility* GetClosestPlatformObject() const;

  BrowserAccessibility* GetPreviousSibling() const;
  BrowserAccessibility* GetNextSibling() const;

  bool IsPreviousSiblingOnSameLine() const;
  bool IsNextSiblingOnSameLine() const;

  // Returns nullptr if there are no children.
  BrowserAccessibility* PlatformDeepestFirstChild() const;
  // Returns nullptr if there are no children.
  BrowserAccessibility* PlatformDeepestLastChild() const;

  // Returns nullptr if there are no children.
  BrowserAccessibility* InternalDeepestFirstChild() const;
  // Returns nullptr if there are no children.
  BrowserAccessibility* InternalDeepestLastChild() const;

  // Returns the bounds of this object in coordinates relative to this frame.
  gfx::Rect GetFrameBoundsRect() const;

  // Returns the bounds of this object in coordinates relative to the
  // page (specifically, the top-left corner of the topmost web contents).
  // Optionally updates |offscreen| to be true if the element is offscreen
  // within its page.
  gfx::Rect GetPageBoundsRect(bool* offscreen = nullptr) const;

  // Returns the bounds of the given range in coordinates relative to the
  // top-left corner of the overall web area. Only valid when the
  // role is WebAXRoleStaticText.
  gfx::Rect GetPageBoundsForRange(int start, int len) const;

  // Same as |GetPageBoundsForRange| but in screen coordinates.
  gfx::Rect GetScreenBoundsForRange(int start, int len) const;

  // Convert a bounding rectangle from this node's coordinate system
  // (which is relative to its nearest scrollable ancestor) to
  // absolute bounds, either in page coordinates (when |frameOnly| is
  // false), or in frame coordinates (when |frameOnly| is true).
  // Updates optional |offscreen| to be true if the node is offscreen.
  virtual gfx::Rect RelativeToAbsoluteBounds(gfx::RectF bounds,
                                             bool frame_only,
                                             bool* offscreen = nullptr) const;

  // This is to handle the cases such as ARIA textbox, where the value should
  // be calculated from the object's inner text.
  virtual base::string16 GetValue() const;

  // This is an approximate hit test that only uses the information in
  // the browser process to compute the correct result. It will not return
  // correct results in many cases of z-index, overflow, and absolute
  // positioning, so BrowserAccessibilityManager::CachingAsyncHitTest
  // should be used instead, which falls back on calling ApproximateHitTest
  // automatically.
  BrowserAccessibility* ApproximateHitTest(const gfx::Point& screen_point);

  // Marks this object for deletion, releases our reference to it, and
  // nulls out the pointer to the underlying AXNode.  May not delete
  // the object immediately due to reference counting.
  //
  // Reference counting is used on some platforms because the
  // operating system may hold onto a reference to a BrowserAccessibility
  // object even after we're through with it. When a BrowserAccessibility
  // has had Destroy() called but its reference count is not yet zero,
  // instance_active() returns false and queries on this object return failure.
  virtual void Destroy();

  // Subclasses should override this to support platform reference counting.
  virtual void NativeAddReference() { }

  // Subclasses should override this to support platform reference counting.
  virtual void NativeReleaseReference();

  //
  // Accessors
  //

  BrowserAccessibilityManager* manager() const { return manager_; }
  bool instance_active() const { return node_ && manager_; }
  ui::AXNode* node() const { return node_; }

  // These access the internal accessibility tree, which doesn't necessarily
  // reflect the accessibility tree that should be exposed on each platform.
  // Use PlatformChildCount and PlatformGetChild to implement platform
  // accessibility APIs.
  uint32_t InternalChildCount() const;
  BrowserAccessibility* InternalGetChild(uint32_t child_index) const;
  BrowserAccessibility* InternalGetParent() const;
  BrowserAccessibility* PlatformGetParent() const;

  int32_t GetId() const;
  gfx::RectF GetLocation() const;
  ui::AXRole GetRole() const;
  int32_t GetState() const;

  typedef base::StringPairs HtmlAttributes;
  const HtmlAttributes& GetHtmlAttributes() const;

  // Returns true if this is a native platform-specific object, vs a
  // cross-platform generic object. Don't call ToBrowserAccessibilityXXX if
  // IsNative returns false.
  virtual bool IsNative() const;

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

  bool HasInheritedStringAttribute(ui::AXStringAttribute attribute) const;
  const std::string& GetInheritedStringAttribute(
      ui::AXStringAttribute attribute) const;
  bool GetInheritedStringAttribute(ui::AXStringAttribute attribute,
                                   std::string* value) const;

  base::string16 GetInheritedString16Attribute(
      ui::AXStringAttribute attribute) const;
  bool GetInheritedString16Attribute(ui::AXStringAttribute attribute,
                                     base::string16* value) const;

  bool HasIntAttribute(ui::AXIntAttribute attribute) const;
  int GetIntAttribute(ui::AXIntAttribute attribute) const;
  bool GetIntAttribute(ui::AXIntAttribute attribute, int* value) const;

  bool HasStringAttribute(
      ui::AXStringAttribute attribute) const;
  const std::string& GetStringAttribute(ui::AXStringAttribute attribute) const;
  bool GetStringAttribute(ui::AXStringAttribute attribute,
                          std::string* value) const;

  base::string16 GetString16Attribute(
      ui::AXStringAttribute attribute) const;
  bool GetString16Attribute(ui::AXStringAttribute attribute,
                            base::string16* value) const;

  bool HasIntListAttribute(ui::AXIntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      ui::AXIntListAttribute attribute) const;
  bool GetIntListAttribute(ui::AXIntListAttribute attribute,
                           std::vector<int32_t>* value) const;

  // Retrieve the value of a html attribute from the attribute map and
  // returns true if found.
  bool GetHtmlAttribute(const char* attr, std::string* value) const;
  bool GetHtmlAttribute(const char* attr, base::string16* value) const;

  base::string16 GetFontFamily() const;
  base::string16 GetLanguage() const;

  virtual base::string16 GetText() const;

  // Returns true if the bit corresponding to the given enum is 1.
  bool HasState(ui::AXState state_enum) const;
  bool HasAction(ui::AXAction action_enum) const;

  // Returns true if the caret is active on this object.
  bool HasCaret() const;

  // True if this is a web area, and its grandparent is a presentational iframe.
  bool IsWebAreaForPresentationalIframe() const;

  virtual bool IsClickable() const;
  bool IsNativeTextControl() const;
  bool IsSimpleTextControl() const;
  // Indicates if this object is at the root of a rich edit text control.
  bool IsRichTextControl() const;

  // Return true if the accessible name was explicitly set to "" by the author
  bool HasExplicitlyEmptyName() const;

  // If an object is focusable but has no accessible name, use this
  // to compute a name from its descendants.
  std::string ComputeAccessibleNameFromDescendants() const;

  // Creates a text position rooted at this object.
  AXPlatformPosition::AXPositionInstance CreatePositionAt(
      int offset,
      ui::AXTextAffinity affinity = ui::AX_TEXT_AFFINITY_DOWNSTREAM) const;

  // Gets the text offsets where new lines start.
  std::vector<int> GetLineStartOffsets() const;

  virtual gfx::NativeViewAccessible GetNativeViewAccessible();

  // AXPlatformNodeDelegate.
  const ui::AXNodeData& GetData() const override;
  const ui::AXTreeData& GetTreeData() const override;
  gfx::NativeWindow GetTopLevelWidget() override;
  gfx::NativeViewAccessible GetParent() override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::Rect GetScreenBoundsRect() const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  ui::AXPlatformNode* GetFromNodeID(int32_t id) override;
  int GetIndexInParent() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;

 protected:
  using AXPlatformPositionInstance = AXPlatformPosition::AXPositionInstance;
  using AXPlatformRange = ui::AXRange<AXPlatformPositionInstance::element_type>;

  BrowserAccessibility();

  // The manager of this tree of accessibility objects.
  BrowserAccessibilityManager* manager_;

  // The underlying node.
  ui::AXNode* node_;

  // A unique ID, since node IDs are frame-local.
  int32_t unique_id_;

 private:
  // |GetInnerText| recursively includes all the text from descendants such as
  // text found in any embedded object. In contrast, |GetText| might include a
  // special character in the place of every embedded object instead of its
  // text, depending on the platform.
  base::string16 GetInnerText() const;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibility);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
