// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_android.h"

#include "base/i18n/char_iterator.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/web_contents_accessibility_android.h"
#include "content/common/accessibility_messages.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerAndroid(initial_tree, nullptr, delegate,
                                                factory);
}

BrowserAccessibilityManagerAndroid::BrowserAccessibilityManagerAndroid(
    const ui::AXTreeUpdate& initial_tree,
    WebContentsAccessibilityAndroid* web_contents_accessibility,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      web_contents_accessibility_(web_contents_accessibility),
      prune_tree_for_screen_reader_(true) {
  if (web_contents_accessibility)
    web_contents_accessibility->set_root_manager(this);
  Initialize(initial_tree);
}

BrowserAccessibilityManagerAndroid::~BrowserAccessibilityManagerAndroid() {
  if (web_contents_accessibility_)
    web_contents_accessibility_->set_root_manager(nullptr);
}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerAndroid::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.AddState(ui::AX_STATE_READ_ONLY);

  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}


bool BrowserAccessibilityManagerAndroid::ShouldRespectDisplayedPasswordText() {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  return wcax ? wcax->ShouldRespectDisplayedPasswordText() : false;
}

bool BrowserAccessibilityManagerAndroid::ShouldExposePasswordText() {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  return wcax ? wcax->ShouldExposePasswordText() : false;
}

BrowserAccessibility* BrowserAccessibilityManagerAndroid::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerAndroid::NotifyAccessibilityEvent(
    BrowserAccessibilityEvent::Source source,
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax)
    return;

  if (event_type == ui::AX_EVENT_HIDE)
    return;

  if (event_type == ui::AX_EVENT_TREE_CHANGED)
    return;

  // Layout changes are handled in OnLocationChanges and
  // SendLocationChangeEvents.
  if (event_type == ui::AX_EVENT_LAYOUT_COMPLETE)
    return;

  if (event_type == ui::AX_EVENT_HOVER) {
    HandleHoverEvent(node);
    return;
  }

  // Sometimes we get events on nodes in our internal accessibility tree
  // that aren't exposed on Android. Update |node| to point to the highest
  // ancestor that's a leaf node.
  BrowserAccessibility* original_node = node;
  node = node->GetClosestPlatformObject();
  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  // If the closest platform object is a password field, the event we're
  // getting is doing something in the shadow dom, for example replacing a
  // character with a dot after a short pause. On Android we don't want to
  // fire an event for those changes, but we do want to make sure our internal
  // state is correct, so we call OnDataChanged() and then return.
  if (android_node->IsPassword() && original_node != node) {
    android_node->OnDataChanged();
    return;
  }

  // Always send AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED to notify
  // the Android system that the accessibility hierarchy rooted at this
  // node has changed.
  wcax->HandleContentChanged(node->unique_id());

  // Ignore load complete events on iframes.
  if (event_type == ui::AX_EVENT_LOAD_COMPLETE &&
      node->manager() != GetRootManager()) {
    return;
  }

  switch (event_type) {
    case ui::AX_EVENT_LOAD_COMPLETE:
      wcax->HandlePageLoaded(GetFocus()->unique_id());
      break;
    case ui::AX_EVENT_FOCUS:
      wcax->HandleFocusChanged(node->unique_id());
      break;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      wcax->HandleCheckStateChanged(node->unique_id());
      break;
    case ui::AX_EVENT_CLICKED:
      wcax->HandleClicked(node->unique_id());
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      wcax->HandleScrollPositionChanged(node->unique_id());
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      wcax->HandleScrolledToAnchor(node->unique_id());
      break;
    case ui::AX_EVENT_ALERT:
      // An alert is a special case of live region. Fall through to the
      // next case to handle it.
    case ui::AX_EVENT_SHOW: {
      // This event is fired when an object appears in a live region.
      // Speak its text.
      base::string16 text = android_node->GetText();
      wcax->AnnounceLiveRegionText(text);
      break;
    }
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
      wcax->HandleTextSelectionChanged(node->unique_id());
      break;
    case ui::AX_EVENT_TEXT_CHANGED:
    case ui::AX_EVENT_VALUE_CHANGED:
      if (android_node->IsEditableText() && GetFocus() == node) {
        wcax->HandleEditableTextChanged(node->unique_id());
      } else if (android_node->IsSlider()) {
        wcax->HandleSliderChanged(node->unique_id());
      }
      break;
    default:
      // There are some notifications that aren't meaningful on Android.
      // It's okay to skip them.
      break;
  }
}

void BrowserAccessibilityManagerAndroid::SendLocationChangeEvents(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  // Android is not very efficient at handling notifications, and location
  // changes in particular are frequent and not time-critical. If a lot of
  // nodes changed location, just send a single notification after a short
  // delay (to batch them), rather than lots of individual notifications.
  if (params.size() > 3) {
    auto* wcax = GetWebContentsAXFromRootManager();
    if (!wcax)
      return;
    wcax->SendDelayedWindowContentChangedEvent();
  }
  BrowserAccessibilityManager::SendLocationChangeEvents(params);
}

bool BrowserAccessibilityManagerAndroid::NextAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      base::string16 text = node->GetText();
      if (cursor_index >= static_cast<int32_t>(text.length()))
        return false;
      base::i18n::UTF16CharIterator iter(text.data(), text.size());
      while (!iter.end() && iter.array_pos() <= cursor_index)
        iter.Advance();
      *start_index = iter.array_pos();
      *end_index = iter.array_pos();
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0)
        return false;

      size_t index = 0;
      while (index < starts.size() - 1 && starts[index] < cursor_index)
        index++;

      if (starts[index] < cursor_index)
        return false;

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED();
  }

  return true;
}

bool BrowserAccessibilityManagerAndroid::PreviousAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      if (cursor_index <= 0)
        return false;
      base::string16 text = node->GetText();
      base::i18n::UTF16CharIterator iter(text.data(), text.size());
      int previous_index = 0;
      while (!iter.end() && iter.array_pos() < cursor_index) {
        previous_index = iter.array_pos();
        iter.Advance();
      }
      *start_index = previous_index;
      *end_index = previous_index;
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0)
        return false;

      size_t index = starts.size() - 1;
      while (index > 0 && starts[index] >= cursor_index)
        index--;

      if (starts[index] >= cursor_index)
        return false;

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED();
  }

  return true;
}

bool BrowserAccessibilityManagerAndroid::OnHoverEvent(
    const ui::MotionEventAndroid& event) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  return wcax ? wcax->OnHoverEvent(event) : false;
}

void BrowserAccessibilityManagerAndroid::HandleHoverEvent(
    BrowserAccessibility* node) {
  WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
  if (!wcax)
    return;

  // First walk up to the nearest platform node, in case this node isn't
  // even exposed on the platform.
  node = node->GetClosestPlatformObject();

  // If this node is uninteresting and just a wrapper around a sole
  // interesting descendant, prefer that descendant instead.
  const BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  const BrowserAccessibilityAndroid* sole_interesting_node =
      android_node->GetSoleInterestingNodeFromSubtree();
  if (sole_interesting_node)
    android_node = sole_interesting_node;

  // Finally, if this node is still uninteresting, try to walk up to
  // find an interesting parent.
  while (android_node && !android_node->IsInterestingOnAndroid()) {
    android_node = static_cast<BrowserAccessibilityAndroid*>(
        android_node->PlatformGetParent());
  }

  if (android_node)
    wcax->HandleHover(android_node->unique_id());
}

void BrowserAccessibilityManagerAndroid::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(
      tree, root_changed, changes);

  if (root_changed) {
    WebContentsAccessibilityAndroid* wcax = GetWebContentsAXFromRootManager();
    if (!wcax)
      return;
    wcax->HandleNavigate();
  }
}

bool
BrowserAccessibilityManagerAndroid::UseRootScrollOffsetsWhenComputingBounds() {
  // The Java layer handles the root scroll offset.
  return false;
}

WebContentsAccessibilityAndroid*
BrowserAccessibilityManagerAndroid::GetWebContentsAXFromRootManager() {
  BrowserAccessibility* parent_node = GetParentNodeFromParentTree();
  if (!parent_node)
    return web_contents_accessibility_;

  auto* parent_manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(parent_node->manager());
  return parent_manager->GetWebContentsAXFromRootManager();
}

}  // namespace content
