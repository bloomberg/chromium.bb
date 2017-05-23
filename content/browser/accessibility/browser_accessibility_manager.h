// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/accessibility/ax_platform_position.h"
#include "content/browser/accessibility/browser_accessibility_event.h"
#include "content/common/content_export.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/native_widget_types.h"

struct AccessibilityHostMsg_LocationChangeParams;

namespace content {
class BrowserAccessibility;
class BrowserAccessibilityManager;
#if defined(OS_ANDROID)
class BrowserAccessibilityManagerAndroid;
#elif defined(OS_WIN)
class BrowserAccessibilityManagerWin;
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_X11)
class BrowserAccessibilityManagerAuraLinux;
#elif defined(OS_MACOSX)
class BrowserAccessibilityManagerMac;
#endif

// For testing.
CONTENT_EXPORT ui::AXTreeUpdate MakeAXTreeUpdate(
    const ui::AXNodeData& node,
    const ui::AXNodeData& node2 = ui::AXNodeData(),
    const ui::AXNodeData& node3 = ui::AXNodeData(),
    const ui::AXNodeData& node4 = ui::AXNodeData(),
    const ui::AXNodeData& node5 = ui::AXNodeData(),
    const ui::AXNodeData& node6 = ui::AXNodeData(),
    const ui::AXNodeData& node7 = ui::AXNodeData(),
    const ui::AXNodeData& node8 = ui::AXNodeData(),
    const ui::AXNodeData& node9 = ui::AXNodeData(),
    const ui::AXNodeData& node10 = ui::AXNodeData(),
    const ui::AXNodeData& node11 = ui::AXNodeData(),
    const ui::AXNodeData& node12 = ui::AXNodeData());

// Class that can perform actions on behalf of the BrowserAccessibilityManager.
// Note: BrowserAccessibilityManager should never cache any of the return
// values from any of these interfaces, especially those that return pointers.
// They may only be valid within this call stack. That policy eliminates any
// concerns about ownership and lifecycle issues; none of these interfaces
// transfer ownership and no return values are guaranteed to be valid outside
// of the current call stack.
class CONTENT_EXPORT BrowserAccessibilityDelegate {
 public:
  virtual ~BrowserAccessibilityDelegate() {}

  virtual void AccessibilityPerformAction(const ui::AXActionData& data) = 0;
  virtual bool AccessibilityViewHasFocus() const = 0;
  virtual gfx::Rect AccessibilityGetViewBounds() const = 0;
  virtual gfx::Point AccessibilityOriginInScreen(
      const gfx::Rect& bounds) const = 0;
  virtual float AccessibilityGetDeviceScaleFactor() const = 0;
  virtual void AccessibilityFatalError() = 0;
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() = 0;
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() = 0;
};

class CONTENT_EXPORT BrowserAccessibilityFactory {
 public:
  virtual ~BrowserAccessibilityFactory() {}

  // Create an instance of BrowserAccessibility and return a new
  // reference to it.
  virtual BrowserAccessibility* Create();
};

// This is all of the information about the current find in page result,
// so we can activate it if requested.
struct BrowserAccessibilityFindInPageInfo {
  BrowserAccessibilityFindInPageInfo();

  // This data about find in text results is updated as the user types.
  int request_id;
  int match_index;
  int start_id;
  int start_offset;
  int end_id;
  int end_offset;

  // The active request id indicates that the user committed to a find query,
  // e.g. by pressing enter or pressing the next or previous buttons. If
  // |active_request_id| == |request_id|, we fire an accessibility event
  // to move screen reader focus to that event.
  int active_request_id;
};

// Manages a tree of BrowserAccessibility objects.
class CONTENT_EXPORT BrowserAccessibilityManager : public ui::AXTreeDelegate {
 public:
  // Creates the platform-specific BrowserAccessibilityManager, but
  // with no parent window pointer. Only useful for unit tests.
  static BrowserAccessibilityManager* Create(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  static BrowserAccessibilityManager* FromID(
      ui::AXTreeIDRegistry::AXTreeID ax_tree_id);

  ~BrowserAccessibilityManager() override;

  void Initialize(const ui::AXTreeUpdate& initial_tree);

  static ui::AXTreeUpdate GetEmptyDocument();

  virtual void NotifyAccessibilityEvent(
      BrowserAccessibilityEvent::Source source,
      ui::AXEvent event_type,
      BrowserAccessibility* node);

  // Checks whether focus has changed since the last time it was checked,
  // taking into account whether the window has focus and which frame within
  // the frame tree has focus. If focus has changed, calls FireFocusEvent.
  void FireFocusEventsIfNeeded(BrowserAccessibilityEvent::Source source);

  // Return whether or not we are currently able to fire events.
  virtual bool CanFireEvents();

  // Fire a focus event. Virtual so that some platforms can customize it,
  // like firing a focus event on the root first, on Windows.
  virtual void FireFocusEvent(BrowserAccessibilityEvent::Source source,
                              BrowserAccessibility* node);

  // Return a pointer to the root of the tree, does not make a new reference.
  BrowserAccessibility* GetRoot();

  // Returns a pointer to the BrowserAccessibility object for a given AXNode.
  BrowserAccessibility* GetFromAXNode(const ui::AXNode* node) const;

  // Return a pointer to the object corresponding to the given id,
  // does not make a new reference.
  BrowserAccessibility* GetFromID(int32_t id) const;

  // If this tree has a parent tree, return the parent node in that tree.
  BrowserAccessibility* GetParentNodeFromParentTree();

  // Get the AXTreeData for this frame.
  const ui::AXTreeData& GetTreeData();

  // Called to notify the accessibility manager that its associated native
  // view got focused.
  virtual void OnWindowFocused();

  // Called to notify the accessibility manager that its associated native
  // view lost focus.
  virtual void OnWindowBlurred();

  // Notify the accessibility manager about page navigation.
  void UserIsNavigatingAway();
  virtual void UserIsReloading();
  void NavigationSucceeded();
  void NavigationFailed();

  // Pretend that the given node has focus, for testing only. Doesn't
  // communicate with the renderer and doesn't fire any events.
  void SetFocusLocallyForTesting(BrowserAccessibility* node);

  // For testing only, register a function to be called when focus changes
  // in any BrowserAccessibilityManager.
  static void SetFocusChangeCallbackForTesting(const base::Closure& callback);

  // Normally we avoid firing accessibility focus events when the containing
  // native window isn't focused. However, this can lead to test flakiness
  // because we can't control when a window loses focus, so this provides a
  // way to disable that check for tests.
  static void NeverSuppressFocusEventsForTesting();

  // Accessibility actions. All of these are implemented asynchronously
  // by sending a message to the renderer to perform the respective action
  // on the given node.  See the definition of |ui::AXActionData| for more
  // information about each of these actions.
  void Decrement(const BrowserAccessibility& node);
  void DoDefaultAction(const BrowserAccessibility& node);
  void GetImageData(const BrowserAccessibility& node,
                    const gfx::Size& max_size);
  void HitTest(const gfx::Point& point);
  void Increment(const BrowserAccessibility& node);
  void ScrollToMakeVisible(
      const BrowserAccessibility& node, gfx::Rect subfocus);
  void ScrollToPoint(
      const BrowserAccessibility& node, gfx::Point point);
  void SetFocus(const BrowserAccessibility& node);
  void SetScrollOffset(const BrowserAccessibility& node, gfx::Point offset);
  void SetValue(
      const BrowserAccessibility& node, const base::string16& value);
  void SetSelection(
      ui::AXRange<AXPlatformPosition::AXPositionInstance::element_type> range);
  void SetAccessibilityFocus(const BrowserAccessibility& node);
  void ShowContextMenu(const BrowserAccessibility& node);

  // Retrieve the bounds of the parent View in screen coordinates.
  virtual gfx::Rect GetViewBounds();

  // Fire an event telling native assistive technology to move focus to the
  // given find in page result.
  void ActivateFindInPageResult(int request_id, int match_index);

  // Called when the renderer process has notified us of about tree changes.
  virtual void OnAccessibilityEvents(
      const std::vector<AXEventNotificationDetails>& details);

  // Called when the renderer process updates the location of accessibility
  // objects. Calls SendLocationChangeEvents(), which can be overridden.
  void OnLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);

  // Called when a new find in page result is received. We hold on to this
  // information and don't activate it until the user requests it.
  void OnFindInPageResult(
      int request_id, int match_index, int start_id, int start_offset,
      int end_id, int end_offset);

  // Called in response to a hit test, when the object hit has a child frame
  // (like an iframe element or browser plugin), and we need to do another
  // hit test recursively.
  void OnChildFrameHitTestResult(const gfx::Point& point,
                                 int hit_obj_id,
                                 ui::AXEvent event_to_fire);

  // This is called when the user has committed to a find in page query,
  // e.g. by pressing enter or tapping on the next / previous result buttons.
  // If a match has already been received for this request id,
  // activate the result now by firing an accessibility event. If a match
  // has not been received, we hold onto this request id and update it
  // when OnFindInPageResult is called.
  void ActivateFindInPageResult(int request_id);

#if defined(OS_WIN)
  BrowserAccessibilityManagerWin* ToBrowserAccessibilityManagerWin();
#endif

#if defined(OS_ANDROID)
  BrowserAccessibilityManagerAndroid* ToBrowserAccessibilityManagerAndroid();
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_X11)
  BrowserAccessibilityManagerAuraLinux*
      ToBrowserAccessibilityManagerAuraLinux();
#endif

#if defined(OS_MACOSX)
  BrowserAccessibilityManagerMac* ToBrowserAccessibilityManagerMac();
#endif

  // Return the object that has focus, starting at the top of the frame tree.
  virtual BrowserAccessibility* GetFocus();

  // Return the object that has focus, only considering this frame and
  // descendants.
  BrowserAccessibility* GetFocusFromThisOrDescendantFrame();

  // Given a focused node |focus|, returns a descendant of that node if it
  // has an active descendant, otherwise returns |focus|.
  BrowserAccessibility* GetActiveDescendant(BrowserAccessibility* focus);

  // Returns true if native focus is anywhere in this WebContents or not.
  bool NativeViewHasFocus();

  // True by default, but some platforms want to treat the root
  // scroll offsets separately.
  virtual bool UseRootScrollOffsetsWhenComputingBounds();

  // Walk the tree using depth-first pre-order traversal.
  static BrowserAccessibility* NextInTreeOrder(
      const BrowserAccessibility* object);
  static BrowserAccessibility* PreviousInTreeOrder(
      const BrowserAccessibility* object);
  static BrowserAccessibility* NextTextOnlyObject(
      const BrowserAccessibility* object);
  static BrowserAccessibility* PreviousTextOnlyObject(
      const BrowserAccessibility* object);

  // If the two objects provided have a common ancestor returns both the
  // common ancestor and the child indices of the two subtrees in which the
  // objects are located.
  // Returns false if a common ancestor cannot be found.
  static bool FindIndicesInCommonParent(const BrowserAccessibility& object1,
                                        const BrowserAccessibility& object2,
                                        BrowserAccessibility** common_parent,
                                        int* child_index1,
                                        int* child_index2);

  // Sets |out_is_before| to true if |object1| comes before |object2|
  // in tree order (pre-order traversal), and false if the objects are the
  // same or not in the same tree.
  static ui::AXTreeOrder CompareNodes(
      const BrowserAccessibility& object1,
      const BrowserAccessibility& object2);

  static std::vector<const BrowserAccessibility*> FindTextOnlyObjectsInRange(
      const BrowserAccessibility& start_object,
      const BrowserAccessibility& end_object);

  static base::string16 GetTextForRange(
      const BrowserAccessibility& start_object,
      const BrowserAccessibility& end_object);

  // If start and end offsets are greater than the text's length, returns all
  // the text.
  static base::string16 GetTextForRange(
      const BrowserAccessibility& start_object,
      int start_offset,
      const BrowserAccessibility& end_object,
      int end_offset);

  static gfx::Rect GetPageBoundsForRange(
      const BrowserAccessibility& start_object,
      int start_offset,
      const BrowserAccessibility& end_object,
      int end_offset);

  // Accessors.
  ui::AXTreeIDRegistry::AXTreeID ax_tree_id() const { return ax_tree_id_; }
  float device_scale_factor() const { return device_scale_factor_; }

  // AXTreeDelegate implementation.
  void OnNodeDataWillChange(ui::AXTree* tree,
                            const ui::AXNodeData& old_node_data,
                            const ui::AXNodeData& new_node_data) override;
  void OnTreeDataChanged(ui::AXTree* tree) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeWillBeReparented(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeReparented(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeReparented(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeChanged(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeDelegate::Change>& changes) override;

  BrowserAccessibilityDelegate* delegate() const { return delegate_; }
  void set_delegate(BrowserAccessibilityDelegate* delegate) {
    delegate_ = delegate;
  }

  // If this BrowserAccessibilityManager is a child frame or guest frame,
  // return the BrowserAccessibilityManager from the highest ancestor frame
  // in the frame tree.
  BrowserAccessibilityManager* GetRootManager();

  // Returns the BrowserAccessibilityDelegate from |GetRootManager|, above.
  BrowserAccessibilityDelegate* GetDelegateFromRootManager();

  // Returns whether this is the top document.
  bool IsRootTree();

  // Get a snapshot of the current tree as an AXTreeUpdate.
  ui::AXTreeUpdate SnapshotAXTreeForTesting();

  // Use a custom device scale factor for testing.
  void UseCustomDeviceScaleFactorForTesting(float device_scale_factor);

  // Given a point in screen coordinates, trigger an asynchronous hit test
  // but return the best possible match instantly.
  //
  //
  BrowserAccessibility* CachingAsyncHitTest(const gfx::Point& screen_point);

  // Called in response to a hover event, caches the result for the next
  // call to CachingAsyncHitTest().
  void CacheHitTestResult(BrowserAccessibility* hit_test_result);

 protected:
  using AXPlatformPositionInstance = AXPlatformPosition::AXPositionInstance;
  using AXPlatformRange = ui::AXRange<AXPlatformPositionInstance::element_type>;

  BrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

  BrowserAccessibilityManager(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

  // Send platform-specific notifications to each of these objects that
  // their location has changed. This is called by OnLocationChanges
  // after it's updated the internal data structure.
  virtual void SendLocationChangeEvents(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);

 private:
  // The following states keep track of whether or not the
  // on-screen keyboard is allowed to be shown.
  enum OnScreenKeyboardState {
    // Never show the on-screen keyboard because this tab is hidden.
    OSK_DISALLOWED_BECAUSE_TAB_HIDDEN,

    // This tab was just shown, so don't pop-up the on-screen keyboard if a
    // text field gets focus that wasn't the result of an explicit touch.
    OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED,

    // A touch event has occurred within the window, but focus has not
    // explicitly changed. Allow the on-screen keyboard to be shown if the
    // touch event was within the bounds of the currently focused object.
    // Otherwise we'll just wait to see if focus changes.
    OSK_ALLOWED_WITHIN_FOCUSED_OBJECT,

    // Focus has changed within a tab that's already visible. Allow the
    // on-screen keyboard to show anytime that a touch event leads to an
    // editable text control getting focus.
    OSK_ALLOWED
  };

 protected:
  // The object that can perform actions on our behalf.
  BrowserAccessibilityDelegate* delegate_;

  // Factory to create BrowserAccessibility objects (for dependency injection).
  std::unique_ptr<BrowserAccessibilityFactory> factory_;

  // The underlying tree of accessibility objects.
  std::unique_ptr<ui::AXSerializableTree> tree_;

  // A mapping from a node id to its wrapper of type BrowserAccessibility.
  base::hash_map<int32_t, BrowserAccessibility*> id_wrapper_map_;

  // A queue of accessibility events to fire based on changes to the
  // accessibility tree. Each one is an event and a node id.
  std::vector<std::pair<ui::AXEvent, int32_t>> tree_events_;

  // True if the user has initiated a navigation to another page.
  bool user_is_navigating_away_;

  // The on-screen keyboard state.
  OnScreenKeyboardState osk_state_;

  BrowserAccessibilityFindInPageInfo find_in_page_info_;

  // These are only used by the root BrowserAccessibilityManager of a
  // frame tree. Stores the last focused node and last focused manager so
  // that when focus might have changed we can figure out whether we need
  // to fire a focus event.
  //
  // NOTE: these pointers are not cleared, so they should never be
  // dereferenced, only used for comparison.
  BrowserAccessibility* last_focused_node_;
  BrowserAccessibilityManager* last_focused_manager_;

  // These cache the AX tree ID, node ID, and global screen bounds of the
  // last object found by an asynchronous hit test. Subsequent hit test
  // requests that remain within this object's bounds will return the same
  // object, but will also trigger a new asynchronous hit test request.
  int last_hover_ax_tree_id_;
  int last_hover_node_id_;
  gfx::Rect last_hover_bounds_;

  // True if the root's parent is in another accessibility tree and that
  // parent's child is the root. Ensures that the parent node is notified
  // once when this subtree is first connected.
  bool connected_to_parent_tree_node_;

  // The global ID of this accessibility tree.
  ui::AXTreeIDRegistry::AXTreeID ax_tree_id_;

  // If this tree has a parent tree, this is the cached ID of the parent
  // node within that parent tree. It's computed as needed and cached for
  // speed so that it can be accessed quickly if it hasn't changed.
  int parent_node_id_from_parent_tree_;

  // The device scale factor for the view associated with this frame,
  // cached each time there's any update to the accessibility tree.
  float device_scale_factor_;

  // For testing only: If true, the manually-set device scale factor will be
  // used and it won't be updated from the delegate.
  bool use_custom_device_scale_factor_for_testing_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
