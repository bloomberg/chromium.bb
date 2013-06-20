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

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace {

// Restricts |val| to the range [min, max].
int Clamp(int val, int min, int max) {
  return std::min(std::max(val, min), max);
}

}  // anonymous namespace

namespace content {

namespace aria_strings {
  const char kAriaLivePolite[] = "polite";
  const char kAriaLiveAssertive[] = "assertive";
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerAndroid(ScopedJavaLocalRef<jobject>(),
                                                src, delegate, factory);
}

BrowserAccessibilityManagerAndroid::BrowserAccessibilityManagerAndroid(
    ScopedJavaLocalRef<jobject> content_view_core,
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(src, delegate, factory) {
  if (content_view_core.is_null())
    return;

  // TODO(aboxhall): set up Java references
}

BrowserAccessibilityManagerAndroid::~BrowserAccessibilityManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // TODO(aboxhall): tear down Java references
}

// static
AccessibilityNodeData BrowserAccessibilityManagerAndroid::GetEmptyDocument() {
  AccessibilityNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = AccessibilityNodeData::ROLE_ROOT_WEB_AREA;
  empty_document.state = 1 << AccessibilityNodeData::STATE_READONLY;
  return empty_document;
}

void BrowserAccessibilityManagerAndroid::NotifyAccessibilityEvent(
    int type,
    BrowserAccessibility* node) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // TODO(aboxhall): call into appropriate Java method for each type of
  // notification
}

jint BrowserAccessibilityManagerAndroid::GetRootId(JNIEnv* env, jobject obj) {
  return static_cast<jint>(root_->renderer_id());
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

  if (!node->ComputeName().empty()) {
    if (distance < *nearest_distance) {
      *nearest_candidate = node;
      *nearest_distance = distance;
    }
    return;
  }

  if (!node->IsLeaf()) {
    for (uint32 i = 0; i < node->child_count(); i++) {
      BrowserAccessibility* child = node->GetChild(i);
      FuzzyHitTestImpl(x, y, child, nearest_candidate, nearest_distance);
    }
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

jint BrowserAccessibilityManagerAndroid::GetNativeNodeById(
    JNIEnv* env, jobject obj, jint id) {
  return reinterpret_cast<jint>(GetFromRendererID(id));
}

void BrowserAccessibilityManagerAndroid::NotifyRootChanged() {
  // TODO(aboxhall): non-stub implementation
}

bool
BrowserAccessibilityManagerAndroid::UseRootScrollOffsetsWhenComputingBounds() {
  // The Java layer handles the root scroll offset.
  return false;
}

bool RegisterBrowserAccessibilityManager(JNIEnv* env) {
  // TODO(aboxhall): non-stub implementation
  return false;
}

}  // namespace content
