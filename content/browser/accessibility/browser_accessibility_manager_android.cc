// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_android.h"

#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/common/accessibility_messages.h"
#include "jni/BrowserAccessibilityManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace {

// These are enums from android.view.accessibility.AccessibilityEvent in Java:
enum {
  ANDROID_ACCESSIBILITY_EVENT_TYPE_VIEW_TEXT_CHANGED = 16,
  ANDROID_ACCESSIBILITY_EVENT_TYPE_VIEW_TEXT_SELECTION_CHANGED = 8192
};

enum AndroidHtmlElementType {
  HTML_ELEMENT_TYPE_SECTION,
  HTML_ELEMENT_TYPE_LIST,
  HTML_ELEMENT_TYPE_CONTROL,
  HTML_ELEMENT_TYPE_ANY
};

// Restricts |val| to the range [min, max].
int Clamp(int val, int min, int max) {
  return std::min(std::max(val, min), max);
}

// These are special unofficial strings sent from TalkBack/BrailleBack
// to jump to certain categories of web elements.
AndroidHtmlElementType HtmlElementTypeFromString(base::string16 element_type) {
  if (element_type == base::ASCIIToUTF16("SECTION"))
    return HTML_ELEMENT_TYPE_SECTION;
  else if (element_type == base::ASCIIToUTF16("LIST"))
    return HTML_ELEMENT_TYPE_LIST;
  else if (element_type == base::ASCIIToUTF16("CONTROL"))
    return HTML_ELEMENT_TYPE_CONTROL;
  else
    return HTML_ELEMENT_TYPE_ANY;
}

}  // anonymous namespace

namespace content {

namespace aria_strings {
  const char kAriaLivePolite[] = "polite";
  const char kAriaLiveAssertive[] = "assertive";
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerAndroid(ScopedJavaLocalRef<jobject>(),
                                                src, delegate, factory);
}

BrowserAccessibilityManagerAndroid*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerAndroid() {
  return static_cast<BrowserAccessibilityManagerAndroid*>(this);
}

BrowserAccessibilityManagerAndroid::BrowserAccessibilityManagerAndroid(
    ScopedJavaLocalRef<jobject> content_view_core,
    const ui::AXNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(src, delegate, factory) {
  SetContentViewCore(content_view_core);
}

BrowserAccessibilityManagerAndroid::~BrowserAccessibilityManagerAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_BrowserAccessibilityManager_onNativeObjectDestroyed(env, obj.obj());
}

// static
ui::AXNodeData BrowserAccessibilityManagerAndroid::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state = 1 << ui::AX_STATE_READ_ONLY;
  return empty_document;
}

void BrowserAccessibilityManagerAndroid::SetContentViewCore(
    ScopedJavaLocalRef<jobject> content_view_core) {
  if (content_view_core.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  java_ref_ = JavaObjectWeakGlobalRef(
      env, Java_BrowserAccessibilityManager_create(
          env, reinterpret_cast<intptr_t>(this),
          content_view_core.obj()).obj());
}

void BrowserAccessibilityManagerAndroid::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  if (event_type == ui::AX_EVENT_HIDE)
    return;

  // Always send AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED to notify
  // the Android system that the accessibility hierarchy rooted at this
  // node has changed.
  Java_BrowserAccessibilityManager_handleContentChanged(
      env, obj.obj(), node->renderer_id());

  switch (event_type) {
    case ui::AX_EVENT_LOAD_COMPLETE:
      Java_BrowserAccessibilityManager_handlePageLoaded(
          env, obj.obj(), focus_->renderer_id());
      break;
    case ui::AX_EVENT_FOCUS:
      Java_BrowserAccessibilityManager_handleFocusChanged(
          env, obj.obj(), node->renderer_id());
      break;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      Java_BrowserAccessibilityManager_handleCheckStateChanged(
          env, obj.obj(), node->renderer_id());
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      Java_BrowserAccessibilityManager_handleScrollPositionChanged(
          env, obj.obj(), node->renderer_id());
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      Java_BrowserAccessibilityManager_handleScrolledToAnchor(
          env, obj.obj(), node->renderer_id());
      break;
    case ui::AX_EVENT_ALERT:
      // An alert is a special case of live region. Fall through to the
      // next case to handle it.
    case ui::AX_EVENT_SHOW: {
      // This event is fired when an object appears in a live region.
      // Speak its text.
      BrowserAccessibilityAndroid* android_node =
          static_cast<BrowserAccessibilityAndroid*>(node);
      Java_BrowserAccessibilityManager_announceLiveRegionText(
          env, obj.obj(),
          base::android::ConvertUTF16ToJavaString(
              env, android_node->GetText()).obj());
      break;
    }
    case ui::AX_EVENT_SELECTED_TEXT_CHANGED:
      Java_BrowserAccessibilityManager_handleTextSelectionChanged(
          env, obj.obj(), node->renderer_id());
      break;
    case ui::AX_EVENT_CHILDREN_CHANGED:
    case ui::AX_EVENT_TEXT_CHANGED:
    case ui::AX_EVENT_VALUE_CHANGED:
      if (node->IsEditableText()) {
        Java_BrowserAccessibilityManager_handleEditableTextChanged(
            env, obj.obj(), node->renderer_id());
      }
      break;
    default:
      // There are some notifications that aren't meaningful on Android.
      // It's okay to skip them.
      break;
  }
}

jint BrowserAccessibilityManagerAndroid::GetRootId(JNIEnv* env, jobject obj) {
  return static_cast<jint>(root_->renderer_id());
}

jboolean BrowserAccessibilityManagerAndroid::IsNodeValid(
    JNIEnv* env, jobject obj, jint id) {
  return GetFromRendererID(id) != NULL;
}

jint BrowserAccessibilityManagerAndroid::HitTest(
    JNIEnv* env, jobject obj, jint x, jint y) {
  BrowserAccessibilityAndroid* result =
      static_cast<BrowserAccessibilityAndroid*>(
          root_->BrowserAccessibilityForPoint(gfx::Point(x, y)));

  if (!result)
    return root_->renderer_id();

  if (result->IsFocusable())
    return result->renderer_id();

  // Examine the children of |result| to find the nearest accessibility focus
  // candidate
  BrowserAccessibility* nearest_node = FuzzyHitTest(x, y, result);
  if (nearest_node)
    return nearest_node->renderer_id();

  return root_->renderer_id();
}

jboolean BrowserAccessibilityManagerAndroid::PopulateAccessibilityNodeInfo(
    JNIEnv* env, jobject obj, jobject info, jint id) {
  BrowserAccessibilityAndroid* node = static_cast<BrowserAccessibilityAndroid*>(
      GetFromRendererID(id));
  if (!node)
    return false;

  if (node->parent()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoParent(
        env, obj, info, node->parent()->renderer_id());
  }
  for (unsigned i = 0; i < node->PlatformChildCount(); ++i) {
    Java_BrowserAccessibilityManager_addAccessibilityNodeInfoChild(
        env, obj, info, node->children()[i]->renderer_id());
  }
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoBooleanAttributes(
      env, obj, info,
      id,
      node->IsCheckable(),
      node->IsChecked(),
      node->IsClickable(),
      node->IsEnabled(),
      node->IsFocusable(),
      node->IsFocused(),
      node->IsPassword(),
      node->IsScrollable(),
      node->IsSelected(),
      node->IsVisibleToUser());
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoClassName(
      env, obj, info,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()).obj());
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoContentDescription(
      env, obj, info,
      base::android::ConvertUTF16ToJavaString(env, node->GetText()).obj(),
      node->IsLink());

  gfx::Rect absolute_rect = node->GetLocalBoundsRect();
  gfx::Rect parent_relative_rect = absolute_rect;
  if (node->parent()) {
    gfx::Rect parent_rect = node->parent()->GetLocalBoundsRect();
    parent_relative_rect.Offset(-parent_rect.OffsetFromOrigin());
  }
  bool is_root = node->parent() == NULL;
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoLocation(
      env, obj, info,
      absolute_rect.x(), absolute_rect.y(),
      parent_relative_rect.x(), parent_relative_rect.y(),
      absolute_rect.width(), absolute_rect.height(),
      is_root);

  // New KitKat APIs
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoKitKatAttributes(
      env, obj, info,
      node->CanOpenPopup(),
      node->IsContentInvalid(),
      node->IsDismissable(),
      node->IsMultiLine(),
      node->AndroidInputType(),
      node->AndroidLiveRegionType());
  if (node->IsCollection()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoCollectionInfo(
        env, obj, info,
        node->RowCount(),
        node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsCollectionItem() || node->IsHeading()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoCollectionItemInfo(
        env, obj, info,
        node->RowIndex(),
        node->RowSpan(),
        node->ColumnIndex(),
        node->ColumnSpan(),
        node->IsHeading());
  }
  if (node->IsRangeType()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoRangeInfo(
        env, obj, info,
        node->AndroidRangeType(),
        node->RangeMin(),
        node->RangeMax(),
        node->RangeCurrentValue());
  }

  return true;
}

jboolean BrowserAccessibilityManagerAndroid::PopulateAccessibilityEvent(
    JNIEnv* env, jobject obj, jobject event, jint id, jint event_type) {
  BrowserAccessibilityAndroid* node = static_cast<BrowserAccessibilityAndroid*>(
      GetFromRendererID(id));
  if (!node)
    return false;

  Java_BrowserAccessibilityManager_setAccessibilityEventBooleanAttributes(
      env, obj, event,
      node->IsChecked(),
      node->IsEnabled(),
      node->IsPassword(),
      node->IsScrollable());
  Java_BrowserAccessibilityManager_setAccessibilityEventClassName(
      env, obj, event,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()).obj());
  Java_BrowserAccessibilityManager_setAccessibilityEventListAttributes(
      env, obj, event,
      node->GetItemIndex(),
      node->GetItemCount());
  Java_BrowserAccessibilityManager_setAccessibilityEventScrollAttributes(
      env, obj, event,
      node->GetScrollX(),
      node->GetScrollY(),
      node->GetMaxScrollX(),
      node->GetMaxScrollY());

  switch (event_type) {
    case ANDROID_ACCESSIBILITY_EVENT_TYPE_VIEW_TEXT_CHANGED:
      Java_BrowserAccessibilityManager_setAccessibilityEventTextChangedAttrs(
          env, obj, event,
          node->GetTextChangeFromIndex(),
          node->GetTextChangeAddedCount(),
          node->GetTextChangeRemovedCount(),
          base::android::ConvertUTF16ToJavaString(
              env, node->GetTextChangeBeforeText()).obj(),
          base::android::ConvertUTF16ToJavaString(env, node->GetText()).obj());
      break;
    case ANDROID_ACCESSIBILITY_EVENT_TYPE_VIEW_TEXT_SELECTION_CHANGED:
      Java_BrowserAccessibilityManager_setAccessibilityEventSelectionAttrs(
          env, obj, event,
          node->GetSelectionStart(),
          node->GetSelectionEnd(),
          node->GetEditableTextLength(),
          base::android::ConvertUTF16ToJavaString(env, node->GetText()).obj());
      break;
    default:
      break;
  }

  // Backwards-compatible fallback for new KitKat APIs.
  Java_BrowserAccessibilityManager_setAccessibilityEventKitKatAttributes(
      env, obj, event,
      node->CanOpenPopup(),
      node->IsContentInvalid(),
      node->IsDismissable(),
      node->IsMultiLine(),
      node->AndroidInputType(),
      node->AndroidLiveRegionType());
  if (node->IsCollection()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventCollectionInfo(
        env, obj, event,
        node->RowCount(),
        node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsCollectionItem() || node->IsHeading()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventCollectionItemInfo(
        env, obj, event,
        node->RowIndex(),
        node->RowSpan(),
        node->ColumnIndex(),
        node->ColumnSpan(),
        node->IsHeading());
  }
  if (node->IsRangeType()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventRangeInfo(
        env, obj, event,
        node->AndroidRangeType(),
        node->RangeMin(),
        node->RangeMax(),
        node->RangeCurrentValue());
  }

  return true;
}

void BrowserAccessibilityManagerAndroid::Click(
    JNIEnv* env, jobject obj, jint id) {
  BrowserAccessibility* node = GetFromRendererID(id);
  if (node)
    DoDefaultAction(*node);
}

void BrowserAccessibilityManagerAndroid::Focus(
    JNIEnv* env, jobject obj, jint id) {
  BrowserAccessibility* node = GetFromRendererID(id);
  if (node)
    SetFocus(node, true);
}

void BrowserAccessibilityManagerAndroid::Blur(JNIEnv* env, jobject obj) {
  SetFocus(root_, true);
}

void BrowserAccessibilityManagerAndroid::ScrollToMakeNodeVisible(
    JNIEnv* env, jobject obj, jint id) {
  BrowserAccessibility* node = GetFromRendererID(id);
  if (node)
    ScrollToMakeVisible(*node, gfx::Rect(node->location().size()));
}

BrowserAccessibility* BrowserAccessibilityManagerAndroid::FuzzyHitTest(
    int x, int y, BrowserAccessibility* start_node) {
  BrowserAccessibility* nearest_node = NULL;
  int min_distance = INT_MAX;
  FuzzyHitTestImpl(x, y, start_node, &nearest_node, &min_distance);
  return nearest_node;
}

// static
void BrowserAccessibilityManagerAndroid::FuzzyHitTestImpl(
    int x, int y, BrowserAccessibility* start_node,
    BrowserAccessibility** nearest_candidate, int* nearest_distance) {
  BrowserAccessibilityAndroid* node =
      static_cast<BrowserAccessibilityAndroid*>(start_node);
  int distance = CalculateDistanceSquared(x, y, node);

  if (node->IsFocusable()) {
    if (distance < *nearest_distance) {
      *nearest_candidate = node;
      *nearest_distance = distance;
    }
    // Don't examine any more children of focusable node
    // TODO(aboxhall): what about focusable children?
    return;
  }

  if (!node->GetText().empty()) {
    if (distance < *nearest_distance) {
      *nearest_candidate = node;
      *nearest_distance = distance;
    }
    return;
  }

  for (uint32 i = 0; i < node->PlatformChildCount(); i++) {
    BrowserAccessibility* child = node->PlatformGetChild(i);
    FuzzyHitTestImpl(x, y, child, nearest_candidate, nearest_distance);
  }
}

// static
int BrowserAccessibilityManagerAndroid::CalculateDistanceSquared(
    int x, int y, BrowserAccessibility* node) {
  gfx::Rect node_bounds = node->GetLocalBoundsRect();
  int nearest_x = Clamp(x, node_bounds.x(), node_bounds.right());
  int nearest_y = Clamp(y, node_bounds.y(), node_bounds.bottom());
  int dx = std::abs(x - nearest_x);
  int dy = std::abs(y - nearest_y);
  return dx * dx + dy * dy;
}

jint BrowserAccessibilityManagerAndroid::FindElementType(
    JNIEnv* env, jobject obj, jint start_id, jstring element_type_str,
    jboolean forwards) {
  BrowserAccessibility* node = GetFromRendererID(start_id);
  if (!node)
    return 0;

  AndroidHtmlElementType element_type = HtmlElementTypeFromString(
      base::android::ConvertJavaStringToUTF16(env, element_type_str));

  node = forwards ? NextInTreeOrder(node) : PreviousInTreeOrder(node);
  while (node) {
    switch(element_type) {
      case HTML_ELEMENT_TYPE_SECTION:
        if (node->role() == ui::AX_ROLE_ARTICLE ||
            node->role() == ui::AX_ROLE_APPLICATION ||
            node->role() == ui::AX_ROLE_BANNER ||
            node->role() == ui::AX_ROLE_COMPLEMENTARY ||
            node->role() == ui::AX_ROLE_CONTENT_INFO ||
            node->role() == ui::AX_ROLE_HEADING ||
            node->role() == ui::AX_ROLE_MAIN ||
            node->role() == ui::AX_ROLE_NAVIGATION ||
            node->role() == ui::AX_ROLE_SEARCH ||
            node->role() == ui::AX_ROLE_REGION) {
          return node->renderer_id();
        }
        break;
      case HTML_ELEMENT_TYPE_LIST:
        if (node->role() == ui::AX_ROLE_LIST ||
            node->role() == ui::AX_ROLE_GRID ||
            node->role() == ui::AX_ROLE_TABLE ||
            node->role() == ui::AX_ROLE_TREE) {
          return node->renderer_id();
        }
        break;
      case HTML_ELEMENT_TYPE_CONTROL:
        if (static_cast<BrowserAccessibilityAndroid*>(node)->IsFocusable())
          return node->renderer_id();
        break;
      case HTML_ELEMENT_TYPE_ANY:
        // In theory, the API says that an accessibility service could
        // jump to an element by element name, like 'H1' or 'P'. This isn't
        // currently used by any accessibility service, and we think it's
        // better to keep them high-level like 'SECTION' or 'CONTROL', so we
        // just fall back on linear navigation when we don't recognize the
        // element type.
        if (static_cast<BrowserAccessibilityAndroid*>(node)->IsClickable())
          return node->renderer_id();
        break;
    }

    node = forwards ? NextInTreeOrder(node) : PreviousInTreeOrder(node);
  }

  return 0;
}

void BrowserAccessibilityManagerAndroid::OnRootChanged() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_BrowserAccessibilityManager_handleNavigate(env, obj.obj());
}

bool
BrowserAccessibilityManagerAndroid::UseRootScrollOffsetsWhenComputingBounds() {
  // The Java layer handles the root scroll offset.
  return false;
}

bool RegisterBrowserAccessibilityManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
